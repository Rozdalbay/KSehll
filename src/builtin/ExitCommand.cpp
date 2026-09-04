#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

namespace kshell
{

BuiltinResult builtinExit(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    result.exitRequested = true;
    if (args.size() > 1)
    {
        try
        {
            result.exitCode = std::stoi(args[1]);
        }
        catch (...)
        {
            ctx.printError(L"exit: invalid argument");
            result.exitCode = 1;
        }
    }
    ctx.requestExit(true);
    ctx.setExitCode(result.exitCode);
    return result;
}

BuiltinResult builtinQuit(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinExit(ctx, args);
}

} // namespace kshell