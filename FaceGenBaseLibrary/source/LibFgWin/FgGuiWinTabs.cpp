//
// Coypright (c) 2022 Singular Inversions Inc. (facegen.com)
// Use, modification and distribution is subject to the MIT License,
// see accompanying file LICENSE.txt or facegen.com/base_library_license.txt
//
//
// Win32 has no way to dynamically change the background color of specific tabs (ie currently selected).
// The only way to do this is to override the default WM_ERASEBKGND and WM_PAINT and draw them yourself.
// At which point you may as well implement your own tabs.

#include "stdafx.h"

#include "FgGuiApiTabs.hpp"
#include "FgGuiWin.hpp"
#include "FgThrowWindows.hpp"
#include "FgMatrixC.hpp"
#include "FgBounds.hpp"
#include "FgMetaFormat.hpp"

using namespace std;

namespace Fg {

struct  GuiTabsWin : public GuiBaseImpl
{
    GuiTabs                     m_api;
    HWND                        m_tabHwnd;
    HWND                        hwndThis;
    GuiImplPtrs                 m_panes;
    uint                        m_currPane;
    Vec2I                       m_client;
    RECT                        m_dispArea;
    String8                     m_store;

    GuiTabsWin(const GuiTabs & api)
        : m_api(api)
    {
        FGASSERT(m_api.tabs.size()>0);
        for (size_t ii=0; ii<m_api.tabs.size(); ++ii)
            m_panes.push_back(api.tabs[ii].win->getInstance());
        m_currPane = 0;
    }

    ~GuiTabsWin()
    {
        if (!m_store.empty())   // Win32 instance was created
            saveBsaXml(m_store+".xml",m_currPane,false);
    }

    virtual void
    create(HWND parentHwnd,int ident,String8 const & store,DWORD extStyle,bool visible)
    {
//fgout << fgnl << "Tabs::create visible: " << visible << " extStyle: " << extStyle << fgpush;
        if (m_store.empty()) {      // First creation this session so check for saved state
            uint            cp;
            if (loadBsaXml(store+".xml",cp,false))
                if (cp < m_panes.size())
                    m_currPane = cp;
        }
        m_store = store;
        WinCreateChild      cc;
        cc.extStyle = extStyle;
        cc.visible = visible;
        // Without this, tab outline shadowing can disappear:
        cc.useFillBrush = true;
        winCreateChild(parentHwnd,ident,this,cc);
//fgout << fgpop;
    }

    virtual void
    destroy()
    {
        // Automatically destroys children first:
        DestroyWindow(hwndThis);
    }

    virtual Vec2UI
    getMinSize() const
    {
        Vec2UI   max(0);
        for (size_t ii=0; ii<m_panes.size(); ++ii) {
            const GuiTabDef &    tab = m_api.tabs[ii];
            Vec2UI           pad(tab.padLeft+tab.padRight,tab.padTop+tab.padBottom);
            max = cMax(max,m_panes[ii]->getMinSize()+pad);
        }
        return max + Vec2UI(0,37);
    }

    virtual Vec2B
    wantStretch() const
    {
        for (size_t ii=0; ii<m_panes.size(); ++ii)
            if (m_panes[ii]->wantStretch()[0])
                return Vec2B(true,true);
        return Vec2B(false,true);
    }

    virtual void
    updateIfChanged()
    {
//fgout << fgnl << "Tabs::updateIfChanged" << fgpush;
        m_panes[m_currPane]->updateIfChanged();
//fgout << fgpop;
    }

    virtual void
    moveWindow(Vec2I lo,Vec2I sz)
    {
//fgout << fgnl << "Tabs::moveWindow " << lo << "," << sz << fgpush;
        MoveWindow(hwndThis,lo[0],lo[1],sz[0],sz[1],FALSE);
//fgout << fgpop;
    }

    virtual void
    showWindow(bool s)
    {
//fgout << fgnl << "Tabs::showWindow: " << s << fgpush;
        ShowWindow(hwndThis,s ? SW_SHOW : SW_HIDE);
//fgout << fgpop;
    }

    LRESULT
    wndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
    {
        if (msg == WM_CREATE) {
//fgout << fgnl << "Tabs::WM_CREATE" << fgpush;
            hwndThis = hwnd;
            // Creating the panes before the tabs fixes the problem of trackbars not being visible
            // on first paint (almost ... top/bottom arrows don't appear). No idea why.
            // Used to create all tab windows here and turn visibility on and off with selection but as of
            // Windows 10 this approach causes huge latency when moving main window so now we create/destroy
            // each time tab is changed. Used to be the case that WM_SIZE was not sent non-visible windows,
            // but either that is not the case any more or MS has managed to do something even stupider ...
            FGASSERT(m_currPane < m_panes.size());
            m_panes[m_currPane]->create(hwnd,
                int(m_currPane+1),  // Child identifiers start at 1 since 0 taken above. Not used anyway.
                m_store+"_"+toStr(m_currPane),
                NULL,
                true);
            m_tabHwnd = 
                CreateWindowEx(0,
                    WC_TABCONTROL,
                    L"",
                    WS_CHILD | WS_VISIBLE,
                    0,0,0,0,
                    hwnd,
                    0,      // Identifier 0
                    s_guiWin.hinst,
                    NULL);
            TCITEM  tc = {0};
            tc.mask = TCIF_TEXT;
            for (size_t ii=0; ii<m_panes.size(); ++ii) {
                wstring     wstr = m_api.tabs[ii].label.as_wstring();
                wstr += wchar_t(0);
                tc.pszText = &wstr[0];
                TabCtrl_InsertItem(m_tabHwnd,ii,&tc);
            }
            SendMessage(m_tabHwnd,TCM_SETCURSEL,m_currPane,0);
//fgout << fgpop;
            return 0;
        }
        else if (msg == WM_SIZE) {
            m_client = Vec2I(LOWORD(lParam),HIWORD(lParam));
            if (m_client[0] * m_client[1] > 0) {
//fgout << fgnl << "Tabs::WM_SIZE: " << m_api.tabs[0].label << " : " << m_client << fgpush;
                resize(hwnd);
//fgout << fgpop;
            }
            return 0;
        }
        else if (msg == WM_NOTIFY) {
            LPNMHDR lpnmhdr = (LPNMHDR)lParam;
            if (lpnmhdr->code == TCN_SELCHANGE) {
                int     idx = int(SendMessage(m_tabHwnd,TCM_GETCURSEL,0,0));
                // This can apparently be -1 for 'no tab selected':
                if ((idx >= 0) && (size_t(idx) < m_panes.size())) {
//fgout << fgnl << "Tabs::WM_NOTIFY: " << idx << fgpush;
                    if (uint(idx) != m_currPane) {
                        m_panes[m_currPane]->destroy();
                        m_currPane = uint(idx);
                        String8             subStore = m_store + "_" + toStr(m_currPane);
                        m_panes[m_currPane]->create(hwnd,int(m_currPane)+1,subStore,NULL,true);
                        resizeCurrPane();                           // Always required after creation
                        m_panes[m_currPane]->updateIfChanged();     // Required to update win32 state (eg. sliders)
                        InvalidateRect(hwndThis,NULL,TRUE);         // Tested to be necessary
                    }
//fgout << fgpop;
                }
            }
            return 0;
        }
        else if (msg == WM_PAINT) {
//fgout << fgnl << "Tabs::WM_PAINT";
        }
        return DefWindowProc(hwnd,msg,wParam,lParam);
    }

    void
    resizeCurrPane()
    {
        GuiTabDef const &   tab = m_api.tabs[m_currPane];
        Vec2I               lo (m_dispArea.left + tab.padLeft, m_dispArea.top + tab.padTop),
                            hi (m_dispArea.right - tab.padRight,m_dispArea.bottom - tab.padBottom),
                            sz = hi - lo;
        m_panes[m_currPane]->moveWindow(lo,sz);
    }

    void
    resize(HWND)
    {
        // The repaint TRUE argument is only necessary when going from maximized to normal window size,
        // for some reason the tabs are repainted anyway in other situations:
        MoveWindow(m_tabHwnd,0,0,m_client[0],m_client[1],TRUE);
        m_dispArea.left = 0;
        m_dispArea.top = 0;
        m_dispArea.right = m_client[0];
        m_dispArea.bottom = m_client[1];
        SendMessage(m_tabHwnd,
            TCM_ADJUSTRECT,
            NULL,               // Give me the display area for this window area:
            LPARAM(&m_dispArea));
        resizeCurrPane();
    }
};

GuiImplPtr
guiGetOsImpl(const GuiTabs & api)
{return GuiImplPtr(new GuiTabsWin(api)); }

}
