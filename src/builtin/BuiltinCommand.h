#pragma once

#ifndef KSHELL_BUILTINCOMMAND_H
#define KSHELL_BUILTINCOMMAND_H

#include <string>
#include <vector>
#include <functional>

namespace kshell
{

class ShellContext;

struct BuiltinResult
{
    bool handled = true;
    bool exitRequested = false;
    int exitCode = 0;
};

using BuiltinFunction = std::function<BuiltinResult(ShellContext&, const std::vector<std::wstring>&)>;

struct BuiltinEntry
{
    const wchar_t* name;
    BuiltinFunction function;
};

BuiltinResult builtinCd(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinPwd(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinDir(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinLs(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinMkdir(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinRmdir(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinTouch(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinCopy(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinMove(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinDel(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinType(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinCat(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinEcho(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinClear(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinCls(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinSet(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinUnset(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinEnv(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinAlias(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinUnalias(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinHistory(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinJobs(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinKill(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinWhich(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinHelp(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinExit(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinQuit(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinTime(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinDate(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinWhoami(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinHostname(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinSystemInfo(ShellContext& ctx, const std::vector<std::wstring>& args);

} // namespace kshell

#endif // KSHELL_BUILTINCOMMAND_H