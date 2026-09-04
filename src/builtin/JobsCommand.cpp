#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"
#include "utils/PathUtils.h"

#include <windows.h>

#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace kshell
{

BuiltinResult builtinJobs(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;

    ctx.executor().reapJobs();
    const auto& jobs = ctx.executor().getJobs();

    if (jobs.empty())
    {
        ctx.printOutput(L"No active jobs");
        return result;
    }

    for (const auto& job : jobs)
    {
        std::wstringstream ss;
        ss << L"[" << job.id << L"] ";

        switch (job.state)
        {
        case JobState::Running:
            ss << L"Running  ";
            break;
        case JobState::Finished:
            ss << L"Done  ";
            break;
        case JobState::Failed:
            ss << L"Failed  ";
            break;
        case JobState::Terminated:
            ss << L"Terminated  ";
            break;
        }

        ss << job.command;
        if (job.pid != 0)
        {
            ss << L"  (PID " << job.pid << L")";
        }
        ctx.printOutput(ss.str());
    }
    return result;
}

BuiltinResult builtinTime(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;

    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf = {};
    localtime_s(&tmBuf, &timeT);
    std::wstringstream ss;
    ss << std::put_time(&tmBuf, L"%H:%M:%S");
    ctx.printOutput(ss.str());
    return result;
}

BuiltinResult builtinDate(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;

    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf = {};
    localtime_s(&tmBuf, &timeT);
    std::wstringstream ss;
    ss << std::put_time(&tmBuf, L"%Y-%m-%d");
    ctx.printOutput(ss.str());
    return result;
}

BuiltinResult builtinWhoami(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;
    ctx.printOutput(ctx.environment().getUser());
    return result;
}

BuiltinResult builtinHostname(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;
    ctx.printOutput(ctx.environment().getHostname());
    return result;
}

BuiltinResult builtinSystemInfo(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    (void)args;

    std::wstringstream ss;
    ss << L"KShell " << KSHELL_VERSION << L"\n";
    ss << L"Platform: Windows x64\n";
    ss << L"User: " << ctx.environment().getUser() << L"\n";
    ss << L"Computer: " << ctx.environment().getHostname() << L"\n";
    ss << L"Directory: " << ctx.currentDirectory() << L"\n";

    wchar_t winDir[MAX_PATH] = {0};
    ::GetWindowsDirectoryW(winDir, MAX_PATH);
    ss << L"Windows: " << winDir << L"\n";

    ctx.printOutput(ss.str());
    return result;
}

} // namespace kshell