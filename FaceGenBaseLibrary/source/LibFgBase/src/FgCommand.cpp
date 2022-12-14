//
// Coypright (c) 2022 Singular Inversions Inc. (facegen.com)
// Use, modification and distribution is subject to the MIT License,
// see accompanying file LICENSE.txt or facegen.com/base_library_license.txt
//


#include "stdafx.h"

#include "FgStdString.hpp"
#include "FgStdVector.hpp"
#include "FgCommand.hpp"
#include "FgFileSystem.hpp"
#include "FgTime.hpp"
#include "FgSyntax.hpp"
#include "FgBuild.hpp"
#include "FgTestUtils.hpp"
#include "FgParse.hpp"
#include "FgConio.hpp"

using namespace std;

namespace Fg {

static string       s_breadcrumb;
static string       s_annotateTestDir;
static bool         s_keepTempFiles = false;

static string       cmdStr(Cmd const & cmd)
{
    string          si = "\n        " + cmd.name;
    if (!cmd.description.empty()) {
        for (size_t jj=si.size(); jj<24; ++jj)
            si += " ";
        si += "- " + cmd.description + "\n";
    }
    return si;
}

void                doMenu(
    CLArgs              args,
    Cmds const &        cmdsUnsorted,
    bool                optionAll,
    bool                optionQuiet,
    bool                optionKeep,
    String const &      notes)
{
    s_breadcrumb += toLower(pathToBase(args[0]).m_str) + "_";
    string              cl,desc;
    if (optionQuiet) {
        cl +=   "[-s] ";
        desc += "    -s              - Suppress standard output except for errors.\n";
    }
    if (optionKeep) {
        cl +=   "[-k[<desc>]] ";
        desc += "    -k[<desc>]      - Keep test files in dated test directory [suffixed with <desc>].\n";
    }
    if (optionAll) {
        cl +=   "(<command> | all)\n";
        desc += "    -a              - Automated, no interactive feedback or regression updates\n";
    }
    else
        cl += "<command>\n";
    desc += "    <command>:";
    Cmds                cmds = cmdsUnsorted;
    sort(cmds.begin(),cmds.end());
    for (size_t ii=0; ii<cmds.size(); ++ii)
        desc += cmdStr(cmds[ii]);
    if (!notes.empty())
        desc += "\nNOTES:\n    " + notes;
    Syntax              syn {args,cl+desc};
    while (syn.peekNext()[0] == '-') {
        string              opt = syn.next();
        if ((opt == "-s") && optionQuiet)
            fgout.setDefOut(false);
        else if ((opt.substr(0,2) == "-k") && optionKeep) {
            s_keepTempFiles = true;
            if (opt.size() > 2)
                s_annotateTestDir = opt.substr(2);
        }
        else
            syn.error("Invalid option");
    }
    string              cmd = syn.next(),
                        cmdl = toLower(cmd);
    if (optionAll) {
        if (cmdl == "all") {
            fgout << fgnl << "Testing: " << fgpush;
            for (size_t ii=0; ii<cmds.size(); ++ii) {
                fgout << fgnl << cmds[ii].name << ": " << fgpush;
                cmds[ii].func(svec(cmds[ii].name,cmdl));      // Pass on the 'all'
                fgout << fgpop;
            }
            fgout << fgpop << fgnl << "All Passed.";
            return;
        }
    }
    for (size_t ii=0; ii<cmds.size(); ++ii) {
        if (cmdl == toLower(cmds[ii].name)) {
            cmds[ii].func(syn.rest());
            if (optionAll && (cmdl == "all"))
                fgout << fgpop << fgnl << "Passed.";
            return;
        }
    }
    if (optionAll && (cmdl == "all"))
        fgout << fgpop;
    syn.error("Invalid command",cmd);
}

static String8 s_rootTestDir;

TestDir::TestDir(String const & name)
{
    FGASSERT(!name.empty());
    if (s_rootTestDir.empty()) {
        path = Path(dataDir());
        path.dirs.back() = "_log";      // replace 'data' with 'log'
    }
    else
        path = s_rootTestDir;
    path.dirs.push_back(s_breadcrumb+name);
    string          dt = getDateTimeFilename();
    if (s_annotateTestDir.size() > 0)
        dt += " " + s_annotateTestDir;
    path.dirs.push_back(dt);
    createPath(path.dir());
    pd.push(path.str());
    if (s_keepTempFiles)
        fgout.logFile("_log.txt");
}

TestDir::~TestDir()
{
    if (!pd.orig.empty()) {
        pd.pop();
        if (!s_keepTempFiles) {
            // Recursively delete the directory for this test:
            deleteDirectoryRecursive(path.str());
            // Remove the test name directory if empty:
            path.dirs.resize(path.dirs.size()-1);
            removeDirectory(path.str());
        }
    }
}

void                fgSetRootTestDir(String8 const & dir) { s_rootTestDir = dir; }

void                copyFileToCurrentDir(String const & relPath)
{
    String8        name = pathToName(relPath);
    if (pathExists(name))
        fgThrow("Test copy filename collision",name);
    String8        source = dataDir() + relPath;
    fileCopy(source,name);
}

void                runCmd(CmdFunc const & func,String const & argStr)
{
    fgout << fgnl << argStr << " " << fgpush;
    func(splitChar(argStr));
    fgout << fgpop;
}

bool                fgKeepTempFiles() {return s_keepTempFiles; }

}
