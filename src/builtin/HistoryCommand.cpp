#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"

#include <sstream>

namespace kshell
{

BuiltinResult builtinHistory(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() > 1 && args[1] == L"-c")
    {
        ctx.history().clear();
        ctx.printSuccess(L"History cleared");
        return result;
    }

    const auto& entries = ctx.history().entries();
    if (entries.empty())
    {
        ctx.printOutput(L"No commands in history");
        return result;
    }

    for (size_t i = 0; i < entries.size(); ++i)
    {
        std::wstringstream ss;
        ss << (i + 1) << L"  " << entries[i];
        ctx.printOutput(ss.str());
    }
    return result;
}

} // namespace kshell