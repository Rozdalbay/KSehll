#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

namespace kshell
{

BuiltinResult builtinTrace(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;
    ctx.trace().setEnabled(true);
    ctx.trace().record(TraceKind::CommandTrace, L"tracing enabled");
    ctx.printOutput(L"Tracing enabled. Switch to the Trace panel to view trace output.");
    return result;
}

BuiltinResult builtinUntrace(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;
    ctx.trace().setEnabled(false);
    ctx.trace().record(TraceKind::CommandTrace, L"tracing disabled");
    ctx.printOutput(L"Tracing disabled.");
    return result;
}

BuiltinResult builtinVars(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;
    const auto vars = ctx.variables().snapshot();
    if (vars.empty())
    {
        ctx.printOutput(L"(no tracked shell variables)");
        return result;
    }
    for (const auto& v : vars)
    {
        ctx.printOutput(v.name + L"=" + v.currentValue);
    }
    return result;
}

BuiltinResult builtinClrtrace(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;
    ctx.trace().clear();
    ctx.printOutput(L"Trace log cleared.");
    return result;
}

} // namespace kshell
