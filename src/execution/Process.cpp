#include "execution/Process.h"

#include <vector>

#include "utils/StringUtils.h"

namespace kshell
{

std::optional<ProcessHandle> Process::create(const ProcessLaunchOptions& options)
{
    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = {};
    si.cb = sizeof(si);

    DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;
    if (!options.inheritHandles)
    {
        creationFlags |= CREATE_NO_WINDOW;
    }

    LPVOID envBlock = nullptr;
    std::vector<WCHAR> envBuffer;
    if (!options.environmentBlock.empty())
    {
        envBuffer = std::vector<WCHAR>(options.environmentBlock.begin(),
                                       options.environmentBlock.end());
        envBuffer.push_back(L'\0');
        envBuffer.push_back(L'\0');
        envBlock = envBuffer.data();
    }

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (options.stdinHandle.valid())
    {
        si.hStdInput = options.stdinHandle.get();
        si.dwFlags |= STARTF_USESTDHANDLES;
    }
    if (options.stdoutHandle.valid())
    {
        si.hStdOutput = options.stdoutHandle.get();
        si.dwFlags |= STARTF_USESTDHANDLES;
    }
    if (options.stderrHandle.valid())
    {
        si.hStdError = options.stderrHandle.get();
        si.dwFlags |= STARTF_USESTDHANDLES;
    }

    if (si.dwFlags & STARTF_USESTDHANDLES)
    {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }

    const std::wstring appName = options.executable.empty() ? L"" : options.executable.c_str();
    std::wstring commandLine = options.commandLine;
    if (!commandLine.empty())
    {
        commandLine.push_back(L'\0');
    }

    BOOL ok = ::CreateProcessW(
        appName.empty() ? nullptr : appName.c_str(),
        commandLine.empty() ? nullptr : commandLine.data(),
        &sa,
        &sa,
        TRUE,
        creationFlags,
        envBlock,
        options.workingDirectory.empty() ? nullptr : options.workingDirectory.c_str(),
        &si,
        &pi
    );

    if (!ok)
    {
        return std::nullopt;
    }

    ProcessHandle handle;
    handle.proc.reset(pi.hProcess);
    handle.thread.reset(pi.hThread);
    handle.pid = pi.dwProcessId;
    return handle;
}

ProcessResult Process::wait(const ProcessHandle& handle, DWORD timeoutMs)
{
    ProcessResult result;
    if (!handle.proc.valid())
    {
        result.succeeded = false;
        return result;
    }

    const DWORD waitResult = ::WaitForSingleObject(handle.proc.get(), timeoutMs);
    if (waitResult == WAIT_TIMEOUT)
    {
        result.succeeded = false;
        return result;
    }

    DWORD exitCode = 0;
    result.succeeded = ::GetExitCodeProcess(handle.proc.get(), &exitCode) != FALSE;
    result.exitCode = (exitCode == STILL_ACTIVE) ? 0 : exitCode;
    return result;
}

bool Process::isRunning(const ProcessHandle& handle)
{
    if (!handle.proc.valid())
    {
        return false;
    }
    DWORD exitCode = 0;
    if (!::GetExitCodeProcess(handle.proc.get(), &exitCode))
    {
        return false;
    }
    return exitCode == STILL_ACTIVE;
}

bool Process::terminate(const ProcessHandle& handle, DWORD exitCode)
{
    if (!handle.proc.valid())
    {
        return false;
    }
    BOOL ok = ::TerminateProcess(handle.proc.get(), exitCode);
    if (ok)
    {
        ::WaitForSingleObject(handle.proc.get(), 3000);
    }
    return ok != FALSE;
}

bool Process::isProcessAlive(DWORD pid)
{
    if (pid == 0)
    {
        return false;
    }
    HANDLE handle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (handle == nullptr)
    {
        return false;
    }
    DWORD exitCode = 0;
    const BOOL ok = ::GetExitCodeProcess(handle, &exitCode);
    ::CloseHandle(handle);
    if (!ok)
    {
        return false;
    }
    return exitCode == STILL_ACTIVE;
}

bool Process::killByPid(DWORD pid)
{
    HANDLE handle = ::OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (handle == nullptr)
    {
        return false;
    }
    const BOOL ok = ::TerminateProcess(handle, 1);
    ::CloseHandle(handle);
    return ok != FALSE;
}

} // namespace kshell
