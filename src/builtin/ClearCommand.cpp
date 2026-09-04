#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

namespace kshell
{

BuiltinResult builtinClear(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;
    ctx.console().clear();
    return result;
}

BuiltinResult builtinCls(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinClear(ctx, args);
}

} // namespace kshell