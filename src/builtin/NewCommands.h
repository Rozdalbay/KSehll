#pragma once

#ifndef KSHELL_NEWCOMMANDS_H
#define KSHELL_NEWCOMMANDS_H

#include <string>
#include <vector>

#include "builtin/BuiltinCommand.h"

namespace kshell
{

BuiltinResult builtinOpen(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinTree(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinWhere(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinFind(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinSearch(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinSysinfo(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinProc(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinTop(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinCalc(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinJson(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinTheme(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinReload(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinFg(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinBg(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinGit(ShellContext& ctx, const std::vector<std::wstring>& args);

} // namespace kshell

#endif // KSHELL_NEWCOMMANDS_H
