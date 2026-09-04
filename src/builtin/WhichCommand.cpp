#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"
#include "utils/PathUtils.h"

#include <vector>

namespace kshell
{

static const std::vector<std::wstring>& builtinNames()
{
    static const std::vector<std::wstring> names = {
        L"help", L"exit", L"quit", L"cd", L"pwd", L"dir", L"ls", L"mkdir",
        L"rmdir", L"touch", L"copy", L"move", L"del", L"type", L"cat",
        L"echo", L"clear", L"cls", L"set", L"unset", L"env", L"alias",
        L"unalias", L"history", L"jobs", L"kill", L"which", L"time",
        L"date", L"whoami", L"hostname", L"systeminfo"
    };
    return names;
}

BuiltinResult builtinWhich(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() < 2)
    {
        ctx.printError(L"which: missing command name");
        result.exitCode = 1;
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring& name = args[i];

        for (const auto& builtinName : builtinNames())
        {
            if (stringutils::equalsIgnoreCase(name, builtinName))
            {
                ctx.printOutput(name + L": builtin command");
                goto next_arg;
            }
        }

        if (pathutils::isAbsolutePath(name) || (name.size() >= 2 && name[1] == L':'))
        {
            if (pathutils::pathExists(name) && pathutils::isExecutableFile(name))
            {
                ctx.printOutput(pathutils::normalizePath(name));
            }
            else
            {
                ctx.printError(L"which: not found: " + name);
                result.exitCode = 1;
            }
        }
        else
        {
            const std::wstring cwd = pathutils::getCurrentDirectory();
            std::vector<std::wstring> dirs = {cwd};
            const auto pathEnv = ctx.environment().getPath();
            for (const auto& dir : pathutils::getPathDirectories(pathEnv))
            {
                if (!dir.empty())
                {
                    dirs.push_back(dir);
                }
            }
            const auto found = pathutils::searchExecutable(name, dirs);
            if (found)
            {
                ctx.printOutput(*found);
            }
            else
            {
                ctx.printError(L"which: not found: " + name);
                result.exitCode = 1;
            }
        }

    next_arg:;
    }
    return result;
}

} // namespace kshell