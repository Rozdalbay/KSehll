#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"

namespace kshell
{

BuiltinResult builtinEcho(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() == 1)
    {
        ctx.printOutput(L"");
        return result;
    }

    bool newLine = true;
    std::wstring output;

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring& arg = args[i];

        if (arg == L"-n" && i == 1)
        {
            newLine = false;
            continue;
        }

        if (arg == L"-e" && i == 1)
        {
            continue;
        }

        if (!output.empty())
        {
            output += L" ";
        }
        output += arg;
    }

    if (newLine)
    {
        ctx.printOutput(output);
    }
    else
    {
        ctx.console().print(output);
    }
    return result;
}

} // namespace kshell