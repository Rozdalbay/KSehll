#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"

#include <windows.h>

namespace kshell
{

BuiltinResult builtinKill(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;

    if (args.size() < 2)
    {
        ctx.printError(L"kill: missing PID or job ID");
        result.exitCode = 1;
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring& arg = args[i];

        if (arg.size() > 1 && arg[0] == L'%')
        {
            int jobId = 0;
            try
            {
                jobId = std::stoi(arg.substr(1));
            }
            catch (...)
            {
                ctx.printError(L"kill: invalid job ID: " + arg);
                result.exitCode = 1;
                continue;
            }

            if (!ctx.executor().killJob(jobId))
            {
                ctx.printError(L"kill: job not found: " + arg);
                result.exitCode = 1;
            }
            continue;
        }

        DWORD pid = 0;
        try
        {
            pid = static_cast<DWORD>(std::stoi(arg));
        }
        catch (...)
        {
            ctx.printError(L"kill: invalid PID: " + arg);
            result.exitCode = 1;
            continue;
        }

        if (pid == 0)
        {
            ctx.printError(L"kill: invalid PID 0");
            result.exitCode = 1;
            continue;
        }

        HANDLE hProcess = ::OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == nullptr)
        {
            ctx.printError(L"kill: process not found: " + arg);
            result.exitCode = 1;
            continue;
        }

        BOOL ok = ::TerminateProcess(hProcess, 1);
        ::CloseHandle(hProcess);

        if (!ok)
        {
            ctx.printError(L"kill: failed to terminate PID " + arg);
            result.exitCode = 1;
        }
    }
    return result;
}

} // namespace kshell