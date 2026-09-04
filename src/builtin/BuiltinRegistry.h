#pragma once

#ifndef KSHELL_BUILTINREGISTRY_H
#define KSHELL_BUILTINREGISTRY_H

#include <string>
#include <vector>

#include "builtin/BuiltinCommand.h"

namespace kshell
{

// Shared registry of built-in commands. Both the classic REPL (Shell) and the
// new TUI shell panes resolve commands through this one table, so new builtins
// work identically everywhere. Each entry also carries short help text so the
// `help` command and the command palette can enumerate commands.
struct BuiltinHelp
{
    const wchar_t* category;
    const wchar_t* oneLiner;
};

struct BuiltinRegistry
{
    std::wstring        name;
    BuiltinFunction     function;
    BuiltinHelp         help;
};

const std::vector<BuiltinRegistry>& builtinRegistry();
BuiltinFunction builtinLookup(const std::wstring& name);

} // namespace kshell

#endif // KSHELL_BUILTINREGISTRY_H
