//
// Coypright (c) 2022 Singular Inversions Inc. (facegen.com)
// Use, modification and distribution is subject to the MIT License,
// see accompanying file LICENSE.txt or facegen.com/base_library_license.txt
//


#include "stdafx.h"

#include "FgString.hpp"
#include "FgStdString.hpp"
#include "FgDiagnostics.hpp"
#include "FgStdVector.hpp"

using namespace std;

namespace Fg {

// Following 2 functions are only needed by Windows and don't work on *nix due to
// different sizeof(wchar_t):
#ifdef _WIN32

String8::String8(const wchar_t * s) : m_str(toUtf8(wstring(s)))
{}

String8::String8(const wstring & s) : m_str(toUtf8(s))
{}

wstring
String8::as_wstring() const
{
    return toUtf16(m_str);
}

#endif

String8 &           String8::operator+=(String8 const & rhs)
{
    m_str += rhs.m_str;
    return *this;
}

String8 &           String8::operator+=(char rhs)
{
    FGASSERT(uint(rhs) < 128U);     // must be ASCII
    m_str += rhs;
    return *this;
}

String8
String8::operator+(String8 const& s) const
{
    return String8(m_str+s.m_str);
}

String8
String8::operator+(char c) const
{
    string      ret = m_str;
    FGASSERT(uchar(c) < 128);
    ret.push_back(c);
    return ret;
}

uint32
String8::operator[](size_t idx) const
{
    return uint32(toUtf32(m_str).at(idx));
}

size_t
String8::size() const
{
    return toUtf32(m_str).size();
}

bool
String8::is_ascii() const
{
    for (size_t ii=0; ii<m_str.size(); ++ii)
        if ((m_str[ii] & 0x80) != 0)
            return false;
    return true;
}

string const &
String8::ascii() const
{
    for (size_t ii=0; ii<m_str.size(); ++ii)
        if ((m_str[ii] & 0x80) != 0)
            fgThrow("Attempt to convert non-ascii utf-8 string to ascii",m_str);
    return m_str;
}

string
String8::as_ascii() const
{
    String32       str(as_utf32());
    string          ret;
    for (size_t ii=0; ii<str.size(); ++ii)
        ret += char(str[ii] & 0x7F);
    return ret;
}

String8
String8::replace(char a, char b) const
{
    FGASSERT((uchar(a) < 128) && (uchar(b) < 128));
    String32           str = toUtf32(m_str);
    char32_t            a32 = a,
                        b32 = b;
    for (char32_t & c : str)
        if (c == a32)
            c = b32;
    return String8(str);
}

String8s
String8::split(char ch) const
{
    String32s            strs = splitAtChar(toUtf32(m_str),char32_t(ch));
    String8s           ret;
    ret.reserve(strs.size());
    for (String32 const & str : strs)
        ret.push_back(String8(str));
    return ret;
}

bool
String8::beginsWith(String8 const & s) const
{
    if(s.m_str.size() > m_str.size())
        return false;
    return (m_str.compare(0,s.m_str.size(),s.m_str) == 0);
}

// Can't put this inline without include file recursive dependency:
bool
String8::endsWith(String8 const & str) const
{return Fg::endsWith(as_utf32(),str.as_utf32()); }

String8
String8::toLower() const
{
    String32       tmp = as_utf32();
    for (size_t ii=0; ii<tmp.size(); ++ii) {
        char32_t    ch = tmp[ii];
        if ((ch > 64) && (ch < 91))
            tmp[ii] = ch + 32;
    }
    return String8(tmp);
}

std::ostream& 
operator<<(std::ostream & os, String8 const & s)
{
    return os << s.m_str;
}

std::istream&
operator>>(std::istream & is, String8 & s)
{
    return is >> s.m_str;
}

String8s
toUstrings(Strings const & strs)
{
    String8s        ret;
    ret.reserve(strs.size());
    for (String const & s : strs)
        ret.push_back(String8(s));
    return ret;
}

String8
fgTr(string const & msg)
{
    // Just a stub for now:
    return msg;
}

String8
removeChars(String8 const & str,uchar chr)
{
    FGASSERT(chr < 128);
    String32       s32 = str.as_utf32(),
                    r32;
    for (size_t ii=0; ii<s32.size(); ++ii)
        if (s32[ii] != chr)
            r32.push_back(s32[ii]);
    return String8(r32);
}

String8
removeChars(String8 const & str,String8 chrs)
{
    String32       s32 = str.as_utf32(),
                    c32 = chrs.as_utf32(),
                    r32;
    for (size_t ii=0; ii<s32.size(); ++ii)
        if (!contains(c32,s32[ii]))
            r32.push_back(s32[ii]);
    return String8(r32);
}

bool
isGlobMatch(String8 const & globStr,String8 const & str)
{
    if (globStr.empty())
        return str.empty();
    if (globStr == "*")
        return true;
    String32           gs = globStr.as_utf32(),
                        ts = str.as_utf32();
    if (gs[0] == '*')
        return endsWith(ts,cRest(gs,1));
    else if (gs.back() == '*')
        return beginsWith(ts,cHead(gs,gs.size()-1));
    return (str == globStr);
}

String8
cSubstr8(String8 const & str,size_t start,size_t size)
{
    String8        ret;
    String32       s = str.as_utf32();
    s = cSubstr(s,start,size);
    ret = String8(s);
    return ret;
}

String8
cRest(String8 const & str,size_t start)
{
    String8        ret;
    String32       s = str.as_utf32();
    s = cRest(s,start);
    ret = String8(s);
    return ret;
}

String8
cat(String8s const & strings,String8 const & separator)
{
    String8        ret;
    for (size_t ii=0; ii<strings.size(); ++ii) {
        ret += strings[ii];
        if (ii < strings.size()-1)
            ret += separator;
    }
    return ret;
}
String8         catDeref(Ptrs<String8> const & stringPtrs,String8 const & separator)
{
    String8             ret;
    size_t              sz = stringPtrs.size();
    for (size_t ii=0; ii<sz; ++ii) {
        ret.m_str.append(stringPtrs[ii]->m_str);
        if (ii+1 < sz)
            ret.m_str.append(separator.m_str);
    }
    return ret;
}

string
fgToVariableName(String8 const & str)
{
    string          ret;
    String32    str32 = str.as_utf32();
    // First character must be alphabetical or underscore:
    if (isalpha(char(str32[0])))
        ret += char(str32[0]);
    else
        ret += '_';
    for (size_t ii=1; ii<str32.size(); ++ii) {
        if (isalnum(char(str32[ii])))
            ret += char(str32[ii]);
        else
            ret += '_';
    }
    return ret;
}

String8
replaceCharWithString(String8 const & in,char32_t from,String8 const to)
{
    String32       in32 = in.as_utf32(),
                    to32 = to.as_utf32(),
                    out;
    for (char32_t ch : in32) {
        if (ch == from)
            out += to32;
        else
            out += ch;
    }
    return String8(out);
}

void
printList(String const & title,Strings const & items,bool numbered)
{
    PushIndent      pi {title};
    for (size_t ii=0; ii<items.size(); ++ii) {
        fgout << fgnl;
        if (numbered)
            fgout << toStrDigits(ii,2);
        fgout << " " << items[ii];
    }
}

}
