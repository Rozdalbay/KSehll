#ifndef KSHELL_PSALIASCOMMANDS_H
#define KSHELL_PSALIASCOMMANDS_H

#include "builtin/BuiltinCommand.h"

#include <string>
#include <vector>

namespace kshell
{

// KShell emulates common PowerShell cmdlets (Verb-Noun notation). These are
// thin wrappers that translate a PowerShell-style argument list (positional +
// named `-Name value`) into the argument form used by KShell's native builtins
// and delegate to those builtins. See PSAliasCommands.cpp for the mapping.

BuiltinResult builtinGetChildItem(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinSetLocation(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetLocation(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetContent(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetProcess(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetDate(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetHelp(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetCommand(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinWriteOutput(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinClearHost(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinCopyItem(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinMoveItem(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinRemoveItem(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinNewItem(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetEnv(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinSetEnv(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetHost(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGetComputerInfo(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinInvokeHistory(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinInvokeJobs(ShellContext& ctx, const std::vector<std::wstring>& args);

} // namespace kshell

#endif // KSHELL_PSALIASCOMMANDS_H
