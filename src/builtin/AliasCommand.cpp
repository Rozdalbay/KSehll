#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"

namespace kshell
{

BuiltinResult builtinAlias(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() == 1)
    {
        const auto aliases = ctx.config().getAliases();
        for (const auto& [name, value] : aliases)
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
            if (value.size() >= 2 &&
                ((value.front() == L'"' && value.back() == L'"') ||
                 (value.front() == L'\'' && value.back() == L'\'')))
            {
                value = value.substr(1, value.size() - 2);
            }
            ctx.config().setAlias(name, value);
        }
        else
        {
            const auto aliases = ctx.config().getAliases();
            const auto it = aliases.find(arg);
            if (it != aliases.end())
            {
                ctx.printOutput(it->first + L"=" + it->second);
            }
            else
            {
                ctx.printError(L"alias: not found: " + arg);
                result.exitCode = 1;
            }
        }
    }
    return result;
}

BuiltinResult builtinUnalias(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() < 2)
    {
        ctx.printError(L"unalias: missing alias name");
        result.exitCode = 1;
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        if (!ctx.config().removeAlias(args[i]))
        {
            ctx.printError(L"unalias: not found: " + args[i]);
            result.exitCode = 1;
        }
    }
    return result;
}

} // namespace kshell