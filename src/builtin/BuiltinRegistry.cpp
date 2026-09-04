#include "builtin/BuiltinRegistry.h"

#include "builtin/NewCommands.h"
#include "builtin/PSAliasCommands.h"
#include "builtin/TraceCommand.h"
#include "utils/StringUtils.h"

namespace kshell
{

const std::vector<BuiltinRegistry>& builtinRegistry()
{
    static const std::vector<BuiltinRegistry> table = {
        {L"help", builtinHelp, {L"GENERAL", L"Show interactive command help"}},
        {L"exit", builtinExit, {L"GENERAL", L"Exit KShell"}},
        {L"quit", builtinQuit, {L"GENERAL", L"Exit KShell"}},
        {L"reload", builtinReload, {L"GENERAL", L"Reload configuration"}},
        {L"theme", builtinTheme, {L"GENERAL", L"List or set the active theme"}},

        {L"cd", builtinCd, {L"FILES", L"Change directory"}},
        {L"pwd", builtinPwd, {L"FILES", L"Print working directory"}},
        {L"dir", builtinDir, {L"FILES", L"List directory contents"}},
        {L"ls", builtinLs, {L"FILES", L"List directory contents"}},
        {L"mkdir", builtinMkdir, {L"FILES", L"Create directory"}},
        {L"rmdir", builtinRmdir, {L"FILES", L"Remove directory"}},
        {L"touch", builtinTouch, {L"FILES", L"Create empty file"}},
        {L"copy", builtinCopy, {L"FILES", L"Copy files"}},
        {L"move", builtinMove, {L"FILES", L"Move files"}},
        {L"del", builtinDel, {L"FILES", L"Delete files"}},
        {L"type", builtinType, {L"FILES", L"Print file contents"}},
        {L"cat", builtinCat, {L"FILES", L"Print file contents"}},
        {L"open", builtinOpen, {L"FILES", L"Open a file/folder/URL with the default app"}},
        {L"tree", builtinTree, {L"FILES", L"Show directory tree"}},
        {L"find", builtinFind, {L"SEARCH", L"Find files by name under a directory"}},
        {L"search", builtinSearch, {L"SEARCH", L"Search file contents for text"}},

        {L"where", builtinWhere, {L"COMMAND", L"Locate a command on PATH"}},
        {L"which", builtinWhich, {L"COMMAND", L"Locate a command on PATH"}},

        {L"jobs", builtinJobs, {L"PROCESS", L"List background jobs"}},
        {L"fg", builtinFg, {L"PROCESS", L"Bring a job to the foreground"}},
        {L"bg", builtinBg, {L"PROCESS", L"Resume/send a job to background"}},
        {L"kill", builtinKill, {L"PROCESS", L"Terminate a process"}},
        {L"proc", builtinProc, {L"PROCESS", L"List processes"}},
        {L"top", builtinTop, {L"PROCESS", L"Show top processes by CPU"}},

        {L"sysinfo", builtinSysinfo, {L"SYSTEM", L"Show system information"}},
        {L"systeminfo", builtinSystemInfo, {L"SYSTEM", L"Show system information"}},

        {L"echo", builtinEcho, {L"GENERAL", L"Print text"}},
        {L"clear", builtinClear, {L"GENERAL", L"Clear the screen"}},
        {L"cls", builtinCls, {L"GENERAL", L"Clear the screen"}},
        {L"set", builtinSet, {L"ENVVAR", L"Set an environment variable"}},
        {L"unset", builtinUnset, {L"ENVVAR", L"Unset an environment variable"}},
        {L"env", builtinEnv, {L"ENVVAR", L"Show environment variables"}},
        {L"alias", builtinAlias, {L"ENVVAR", L"Create/list aliases"}},
        {L"unalias", builtinUnalias, {L"ENVVAR", L"Remove an alias"}},
        {L"history", builtinHistory, {L"HISTORY", L"Show command history"}},
        {L"time", builtinTime, {L"GENERAL", L"Show current time"}},
        {L"trace", builtinTrace, {L"TRACE", L"Enable command tracing"}},
        {L"untrace", builtinUntrace, {L"TRACE", L"Disable command tracing"}},
        {L"vars", builtinVars, {L"ENVVAR", L"List tracked shell variables"}},
        {L"clrtrace", builtinClrtrace, {L"TRACE", L"Clear the trace log"}},
        {L"date", builtinDate, {L"GENERAL", L"Show current date"}},
        {L"whoami", builtinWhoami, {L"GENERAL", L"Show current user"}},
        {L"hostname", builtinHostname, {L"GENERAL", L"Show the computer name"}},
        {L"calc", builtinCalc, {L"GENERAL", L"Evaluate an arithmetic expression"}},
        {L"json", builtinJson, {L"GENERAL", L"Validate and pretty-print a JSON file"}},
        {L"git", builtinGit, {L"GIT", L"Git integration (external git preferred)"}},

        // PowerShell cmdlet emulation (Verb-Noun) -> native builtins.
        {L"Get-ChildItem", builtinGetChildItem, {L"PS", L"Lists directory contents (dir)"}},
        {L"Set-Location", builtinSetLocation, {L"PS", L"Changes directory (cd)"}},
        {L"Get-Location", builtinGetLocation, {L"PS", L"Prints working directory (pwd)"}},
        {L"Get-Content", builtinGetContent, {L"PS", L"Displays file contents (type)"}},
        {L"Get-Process", builtinGetProcess, {L"PS", L"Lists processes (proc)"}},
        {L"Get-Date", builtinGetDate, {L"PS", L"Shows current date (date)"}},
        {L"Get-Help", builtinGetHelp, {L"PS", L"Shows help (help)"}},
        {L"Get-Command", builtinGetCommand, {L"PS", L"Locates a command (where)"}},
        {L"Write-Output", builtinWriteOutput, {L"PS", L"Prints text (echo)"}},
        {L"Clear-Host", builtinClearHost, {L"PS", L"Clears the screen (clear)"}},
        {L"Copy-Item", builtinCopyItem, {L"PS", L"Copies files (copy)"}},
        {L"Move-Item", builtinMoveItem, {L"PS", L"Moves files (move)"}},
        {L"Remove-Item", builtinRemoveItem, {L"PS", L"Deletes files (del)"}},
        {L"New-Item", builtinNewItem, {L"PS", L"Creates a file/directory (touch/mkdir)"}},
        {L"Get-ChildItem Env:", builtinGetEnv, {L"PS", L"Shows environment (env)"}},
        {L"Set-Env", builtinSetEnv, {L"PS", L"Sets an environment variable (set)"}},
        {L"Get-Host", builtinGetHost, {L"PS", L"Shows host/system info (sysinfo)"}},
        {L"Get-ComputerInfo", builtinGetComputerInfo, {L"PS", L"Shows system info (sysinfo)"}},
        {L"Invoke-History", builtinInvokeHistory, {L"PS", L"Shows command history (history)"}},
        {L"Invoke-Job", builtinInvokeJobs, {L"PS", L"Shows background jobs (jobs)"}},
    };
    return table;
}

BuiltinFunction builtinLookup(const std::wstring& name)
{
    for (const auto& entry : builtinRegistry())
    {
        if (stringutils::equalsIgnoreCase(entry.name, name))
        {
            return entry.function;
        }
    }
    return nullptr;
}

} // namespace kshell
