#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include <algorithm>

namespace kshell
{

BuiltinResult builtinHelp(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;

    ctx.console().setColor(ctx.config().colors.successColor);
    ctx.printOutput(L"KShell " KSHELL_VERSION L" - Built-in Commands:");
    ctx.console().resetColor();

    struct Entry
    {
        const wchar_t* cmd;
        const wchar_t* desc;
    };

    const Entry commands[] = {
        {L"help [cmd]",  L"Show this help message"},
        {L"exit / quit", L"Exit KShell"},
        {L"reload",      L"Reload configuration"},
        {L"theme",       L"List or set the active theme"},

        {L"cd [dir]",    L"Change directory"},
        {L"pwd",         L"Print working directory"},
        {L"dir / ls",    L"List directory contents (-a all, -l long)"},
        {L"mkdir",       L"Create directory"},
        {L"rmdir",       L"Remove directory"},
        {L"touch",       L"Create empty file"},
        {L"copy",        L"Copy files"},
        {L"move",        L"Move files"},
        {L"del",         L"Delete files"},
        {L"type / cat",  L"Display file contents"},
        {L"open",        L"Open a file/folder with the default app"},
        {L"tree",        L"Show directory tree"},

        {L"echo",        L"Print text (-n no newline)"},
        {L"clear / cls", L"Clear the screen"},

        {L"set",         L"Show/set environment variables"},
        {L"unset",       L"Remove an environment variable"},
        {L"env",         L"Show all environment variables"},
        {L"alias",       L"Show/set aliases"},
        {L"unalias",     L"Remove an alias"},

        {L"history",     L"Show command history"},
        {L"find",        L"Find files by name"},
        {L"search",      L"Search file contents"},

        {L"jobs",        L"List background jobs"},
        {L"fg / bg",     L"Bring job foreground / resume background"},
        {L"kill",        L"Terminate a process or job"},
        {L"proc",        L"List running processes"},
        {L"top",         L"Show top processes by CPU"},

        {L"which/where", L"Locate a command on PATH"},
        {L"sysinfo",     L"Show system information"},

        {L"time / date", L"Show current time / date"},
        {L"whoami",      L"Show current user"},
        {L"hostname",    L"Show computer name"},
        {L"calc",        L"Evaluate a math expression"},
        {L"json",        L"Validate and pretty-print JSON"},
        {L"git",         L"Git integration"},
    };

    // Compute the widest command column for clean alignment.
    int maxCmdW = 0;
    for (const auto& e : commands)
    {
        int len = (int)wcslen(e.cmd);
        maxCmdW = std::max(maxCmdW, len);
    }

    int cmdColW = maxCmdW + 3;
    for (const auto& e : commands)
    {
        std::wstring line = e.cmd;
        line.append((size_t)(cmdColW - (int)line.size()), L' ');
        line += e.desc;
        ctx.printOutput(line);
    }

    ctx.printOutput(L"");
    ctx.printOutput(L"Pipes:       cmd1 | cmd2");
    ctx.printOutput(L"Redirection: < file  > file  >> file  2> file  2>> file  2>&1");
    ctx.printOutput(L"Background:  cmd &");
    ctx.printOutput(L"Variables:   $NAME  %NAME%");
    return result;
}

} // namespace kshell