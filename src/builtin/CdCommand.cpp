#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"
#include "utils/PathUtils.h"
#include "utils/ErrorUtils.h"

namespace kshell
{

BuiltinResult builtinCd(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() > 2)
    {
        ctx.printError(L"cd: too many arguments");
        result.exitCode = 1;
        return result;
    }

    std::wstring target = (args.size() == 2) ? args[1] : pathutils::getHomeDirectory();

    if (target == L"-" || target == L"~")
    {
        target = pathutils::getHomeDirectory();
    }

    if (ctx.setCurrentDirectory(target))
    {
        ctx.refreshExecutor();
        ctx.trace().record(TraceKind::Directory, L"cd " + target + L" -> " + ctx.currentDirectory());
        result.exitCode = 0;
        return result;
    }

    ctx.printError(L"cd: directory not found:\n" + target);
    result.exitCode = 1;
    return result;
}

BuiltinResult builtinPwd(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    if (args.size() > 1)
    {
        ctx.printError(L"pwd: too many arguments");
        result.exitCode = 1;
        return result;
    }
    ctx.printOutput(ctx.currentDirectory());
    result.exitCode = 0;
    return result;
}

} // namespace kshell