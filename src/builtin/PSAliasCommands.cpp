#include "builtin/PSAliasCommands.h"
#include "builtin/BuiltinCommand.h"
#include "builtin/NewCommands.h"
#include "core/ShellContext.h"
#include "utils/StringUtils.h"
#include "environment/Environment.h"

#include <string>
#include <vector>
#include <algorithm>

namespace kshell
{

namespace
{

// ---------------------------------------------------------------------------
// Helpers to translate PowerShell-style parameter lists.
// ---------------------------------------------------------------------------

// Build a positional KShell-command argument list from cmdlet args:
//  - rename the command (args[0] -> canonicalName)
//  - drop/pull named parameters whose values we fold into positionals
// Returns {canonicalName, positionalArgs...}.
std::vector<std::wstring> toPositional(const std::vector<std::wstring>& args,
                                       const std::wstring& canonicalName)
{
    std::vector<std::wstring> out;
    out.push_back(canonicalName);
    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring& a = args[i];
        if (!a.empty() && a[0] == L'-')
        {
            // Skip a named parameter that carries a following value but the
            // value is handled positionally below; here we only consume flags
            // that map to switches. Values of -Path/-Name/-Value are emitted
            // positionally in their own pass.
            continue;
        }
        out.push_back(a);
    }
    return out;
}

// Extract the value of a named parameter (e.g. -Path "C:\x") and emit it as a
// positional argument, dropping the "-Name value" pair from the list.
// Positional (non-dash) args are preserved in order.
std::vector<std::wstring> pickNamed(const std::vector<std::wstring>& args,
                                    const std::wstring& canonicalName,
                                    const std::vector<std::wstring>& named)
{
    std::vector<std::wstring> out;
    out.push_back(canonicalName);

    auto isNamed = [&](const std::wstring& a) {
        if (a.empty() || a[0] != L'-') return false;
        std::wstring key = a.substr(1);
        for (const auto& n : named)
        {
            if (stringutils::equalsIgnoreCase(key, n)) return true;
        }
        return false;
    };

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring& a = args[i];
        if (isNamed(a))
        {
            if (i + 1 < args.size())
            {
                out.push_back(args[i + 1]);
                ++i;
            }
        }
        else if (!a.empty() && a[0] == L'-')
        {
            // Unhandled flag/switch; skip.
        }
        else
        {
            out.push_back(a);
        }
    }
    return out;
}

// Rebuild a KShell builtin argument vector with a specific name.
std::vector<std::wstring> rehead(const std::vector<std::wstring>& args,
                                 const std::wstring& name)
{
    std::vector<std::wstring> out;
    out.push_back(name);
    for (size_t i = 1; i < args.size(); ++i)
    {
        out.push_back(args[i]);
    }
    return out;
}

} // namespace

// Get-ChildItem [-Path] -> dir / ls
BuiltinResult builtinGetChildItem(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    auto a = pickNamed(args, L"dir", {L"Path", L"LiteralPath"});
    return builtinDir(ctx, a);
}

// Set-Location [-Path] -> cd
BuiltinResult builtinSetLocation(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    // Handle common PowerShell aliases for position changes.
    auto a = pickNamed(args, L"cd", {L"Path", L"LiteralPath"});
    if (a.size() >= 2)
    {
        static const std::wstring home = L"~";
        if (a[1] == home)
        {
            // cd to home dir via env.
            a[1] = ctx.environment().get(L"USERPROFILE").value_or(a[1]);
        }
    }
    return builtinCd(ctx, a);
}

// Get-Location -> pwd
BuiltinResult builtinGetLocation(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinPwd(ctx, rehead(args, L"pwd"));
}

// Get-Content [-Path] -> type
BuiltinResult builtinGetContent(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    auto a = pickNamed(args, L"type", {L"Path", L"LiteralPath"});
    return builtinType(ctx, a);
}

// Get-Process [-Name] -> proc
BuiltinResult builtinGetProcess(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    // proc takes a numeric PID as arg[1]; for Get-Process -Name foo we only
    // list processes (filtering by name is an enhancement, keep simple here).
    std::vector<std::wstring> a = {L"proc"};
    (void)args;
    return builtinProc(ctx, a);
}

// Get-Date -> date / time
BuiltinResult builtinGetDate(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinDate(ctx, rehead(args, L"date"));
}

// Get-Help -> help
BuiltinResult builtinGetHelp(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinHelp(ctx, rehead(args, L"help"));
}

// Get-Command -> where
BuiltinResult builtinGetCommand(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinWhere(ctx, rehead(args, L"where"));
}

// Write-Output -> echo
BuiltinResult builtinWriteOutput(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinEcho(ctx, rehead(args, L"echo"));
}

// Clear-Host -> clear
BuiltinResult builtinClearHost(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinClear(ctx, rehead(args, L"clear"));
}

// Copy-Item -> copy
BuiltinResult builtinCopyItem(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    std::vector<std::wstring> a;
    a.push_back(L"copy");
    // Support -Path a -Destination b.
    std::wstring src, dst;
    for (size_t i = 1; i < args.size(); ++i)
    {
        const auto& p = args[i];
        auto val = [&](size_t idx) { return idx < args.size() ? args[idx] : L""; };
        if (stringutils::equalsIgnoreCase(p, L"-Path") || stringutils::equalsIgnoreCase(p, L"-LiteralPath")) { src = val(i + 1); ++i; }
        else if (stringutils::equalsIgnoreCase(p, L"-Destination")) { dst = val(i + 1); ++i; }
        else if (!p.empty() && p[0] == L'-') { /* skip switch */ }
        else { if (src.empty()) src = p; else if (dst.empty()) dst = p; }
    }
    if (!src.empty() && !dst.empty())
    {
        a.push_back(src);
        a.push_back(dst);
    }
    return builtinCopy(ctx, a);
}

// Move-Item -> move
BuiltinResult builtinMoveItem(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    std::vector<std::wstring> a;
    a.push_back(L"move");
    std::wstring src, dst;
    for (size_t i = 1; i < args.size(); ++i)
    {
        const auto& p = args[i];
        auto val = [&](size_t idx) { return idx < args.size() ? args[idx] : L""; };
        if (stringutils::equalsIgnoreCase(p, L"-Path") || stringutils::equalsIgnoreCase(p, L"-LiteralPath")) { src = val(i + 1); ++i; }
        else if (stringutils::equalsIgnoreCase(p, L"-Destination")) { dst = val(i + 1); ++i; }
        else if (!p.empty() && p[0] == L'-') { /* skip switch */ }
        else { if (src.empty()) src = p; else if (dst.empty()) dst = p; }
    }
    if (!src.empty() && !dst.empty())
    {
        a.push_back(src);
        a.push_back(dst);
    }
    return builtinMove(ctx, a);
}

// Remove-Item -> del
BuiltinResult builtinRemoveItem(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    auto a = pickNamed(args, L"del", {L"Path", L"LiteralPath"});
    return builtinDel(ctx, a);
}

// New-Item [-ItemType Directory] -> mkdir; else touch
BuiltinResult builtinNewItem(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    std::vector<std::wstring> a = {L"mkdir"};
    bool isDir = false;
    for (size_t i = 1; i < args.size(); ++i)
    {
        const auto& p = args[i];
        auto val = [&](size_t idx) { return idx < args.size() ? args[idx] : L""; };
        if (stringutils::equalsIgnoreCase(p, L"-ItemType"))
        {
            std::wstring t = val(i + 1);
            ++i;
            isDir = (stringutils::equalsIgnoreCase(t, L"directory") ||
                     stringutils::equalsIgnoreCase(t, L"dir"));
        }
        else if (stringutils::equalsIgnoreCase(p, L"-Path")) { a.push_back(val(i + 1)); ++i; }
        else if (!p.empty() && p[0] == L'-') { /* skip */ }
        else { a.push_back(p); }
    }
    a[0] = isDir ? L"mkdir" : L"touch";
    if (a.size() < 2)
    {
        return a[0] == L"mkdir" ? builtinMkdir(ctx, a) : builtinTouch(ctx, a);
    }
    return isDir ? builtinMkdir(ctx, a) : builtinTouch(ctx, a);
}

// Get-ChildItem Env: -> env (Get-Env)
BuiltinResult builtinGetEnv(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinEnv(ctx, rehead(args, L"env"));
}

// Set-Env? Not a real cmdlet; add-via-env alias.
BuiltinResult builtinSetEnv(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    // set NAME=value
    std::vector<std::wstring> a = {L"set"};
    std::wstring name, value;
    for (size_t i = 1; i < args.size(); ++i)
    {
        const auto& p = args[i];
        auto val = [&](size_t idx) { return idx < args.size() ? args[idx] : L""; };
        if (stringutils::equalsIgnoreCase(p, L"-Name")) { name = val(i + 1); ++i; }
        else if (stringutils::equalsIgnoreCase(p, L"-Value")) { value = val(i + 1); ++i; }
    }
    if (!name.empty())
    {
        a.push_back(name + L"=" + value);
    }
    return builtinSet(ctx, a);
}

// Get-Host -> sysinfo (computer/user)
BuiltinResult builtinGetHost(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinSysinfo(ctx, rehead(args, L"sysinfo"));
}

// Get-ComputerInfo -> sysinfo
BuiltinResult builtinGetComputerInfo(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinSysinfo(ctx, rehead(args, L"sysinfo"));
}

// Invoke-History -> history
BuiltinResult builtinInvokeHistory(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinHistory(ctx, rehead(args, L"history"));
}

// Invoke-Job -> jobs
BuiltinResult builtinInvokeJobs(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinJobs(ctx, rehead(args, L"jobs"));
}

} // namespace kshell
