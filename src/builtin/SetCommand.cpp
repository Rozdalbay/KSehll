#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"

#include <algorithm>

namespace kshell
{

BuiltinResult builtinSet(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() == 1)
    {
        const auto all = ctx.environment().getAll();
        for (const auto& [name, value] : all)
        {
            ctx.printOutput(name + L"=" + value);
        }
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring& arg = args[i];
        const auto eq = arg.find(L'=');
        if (eq != std::wstring::npos)
        {
            const std::wstring name = arg.substr(0, eq);
            std::wstring value = arg.substr(eq + 1);
            ctx.environment().set(name, value);
            ctx.variables().recordSet(name, value);
            ctx.trace().record(TraceKind::Variable, L"set " + name + L"=" + value);
            ctx.refreshExecutor();
        }
        else
        {
            const auto value = ctx.environment().get(arg);
            if (value)
            {
                ctx.printOutput(arg + L"=" + *value);
            }
            else
            {
                ctx.printError(L"set: not found: " + arg);
            }
        }
    }
    return result;
}

BuiltinResult builtinEnv(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;

    const auto all = ctx.environment().getAll();
    for (const auto& [name, value] : all)
    {
        ctx.printOutput(name + L"=" + value);
    }
    return result;
}

} // namespace kshell