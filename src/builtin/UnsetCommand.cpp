#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

namespace kshell
{

BuiltinResult builtinUnset(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() < 2)
    {
        ctx.printError(L"unset: missing variable name");
        result.exitCode = 1;
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        ctx.environment().unset(args[i]);
        ctx.variables().recordUnset(args[i]);
        ctx.trace().record(TraceKind::Variable, L"unset " + args[i]);
        ctx.refreshExecutor();
    }
    return result;
}

} // namespace kshell