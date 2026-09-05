#include "ui/ExecEngine.h"

#include <windows.h>

#include <thread>

namespace kshell::ui
{

namespace
{

CancelPoll g_cancelPoll = nullptr;

// Read a pipe fully into a UTF-8 string, then convert to wide text.
// Runs on its own thread so a chatty process can't deadlock the capture.
void pipeToString(HANDLE readPipe, std::wstring& out)
{
    std::string bytes;
    char buf[4096];
    DWORD read = 0;
    for (;;)
    {
        if (!::ReadFile(readPipe, buf, sizeof(buf), &read, nullptr) || read == 0)
        {
            break;
        }
        bytes.append(buf, read);
    }
    if (!bytes.empty())
    {
        int wlen = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
        if (wlen <= 0)
        {
            wlen = ::MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
        }
        if (wlen > 0)
        {
            out.resize((size_t)wlen);
            ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), &out[0], wlen);
        }
    }
}

CaptureResult launchAndCapture(const std::wstring& app,
                                const std::wstring& cmdLine,
                                const std::wstring& cwd,
                                const std::wstring& envBlock,
                                HANDLE extraStdin)
{
    CaptureResult result;
    LARGE_INTEGER freq{};
    LARGE_INTEGER start{};
    ::QueryPerformanceFrequency(&freq);
    ::QueryPerformanceCounter(&start);

    HANDLE hStdoutRead = nullptr;
    HANDLE hStdoutWrite = nullptr;
    HANDLE hStderrRead = nullptr;
    HANDLE hStderrWrite = nullptr;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    ::CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0);
    ::CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0);
    ::SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = extraStdin ? extraStdin : ::GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    const wchar_t* cwdPtr = cwd.empty() ? nullptr : cwd.c_str();
    const wchar_t* envPtr = envBlock.empty() ? nullptr : envBlock.c_str();
    LPVOID envMutable = const_cast<LPVOID>(static_cast<const void*>(envPtr));

    // Pass lpApplicationName when we have an explicit exe path (e.g. cmd.exe)
    // so that Windows resolves the executable correctly instead of trying to
    // parse it from the command line.
    // Pass lpApplicationName only when we have an explicit exe path so that
    // Windows resolves the executable correctly instead of trying to parse it
    // from the command line. A bare program name (no path separators) must go
    // through PATH resolution instead: CreateProcessW does NOT search PATH when
    // lpApplicationName is supplied and will fail with ERROR_FILE_NOT_FOUND.
    const bool appHasPath = (app.find_first_of(L"\\/") != std::wstring::npos);
    const wchar_t* appPtr = (!app.empty() && appHasPath) ? app.c_str() : nullptr;

    DWORD flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW;

    BOOL ok = ::CreateProcessW(appPtr, mutableCmd.data(), nullptr, nullptr, TRUE,
                               flags, envMutable, cwdPtr, &si, &pi);

    ::CloseHandle(hStdoutWrite);
    ::CloseHandle(hStderrWrite);

    if (extraStdin)
    {
        ::CloseHandle(extraStdin);
    }

    if (!ok)
    {
        ::CloseHandle(hStdoutRead);
        ::CloseHandle(hStderrRead);
        result.exitCode = (int)::GetLastError();
        return result;
    }

    ::CloseHandle(pi.hThread);

    std::wstring stdoutText;
    std::wstring stderrText;

    // Read stdout and stderr on separate threads so neither pipe can fill up
    // and deadlock, and so we can interrupt a hung process below.
    std::thread readerOut([&hStdoutRead, &stdoutText] { pipeToString(hStdoutRead, stdoutText); });
    std::thread readerErr([&hStderrRead, &stderrText] { pipeToString(hStderrRead, stderrText); });

    // Poll the process instead of blocking forever: a hung command (e.g. git
    // waiting on the network or a credential prompt) must not freeze the UI.
    bool   cancelled = false;
    DWORD  exitCode = 0;
    DWORD  waitedMs = 0;
    const DWORD kWaitLimitMs = 300000;
    for (;;)
    {
        const DWORD wait = ::WaitForSingleObject(pi.hProcess, 50);
        if (wait == WAIT_OBJECT_0)
        {
            break;
        }
        if (wait == WAIT_TIMEOUT)
        {
            waitedMs += 50;
            if (g_cancelPoll && g_cancelPoll())
            {
                ::TerminateProcess(pi.hProcess, 1);
                cancelled = true;
                break;
            }
            if (waitedMs >= kWaitLimitMs)
            {
                ::TerminateProcess(pi.hProcess, 1);
                break;
            }
            continue;
        }
        break;
    }

    ::GetExitCodeProcess(pi.hProcess, &exitCode);
    ::CloseHandle(pi.hProcess);

    readerOut.join();
    readerErr.join();
    ::CloseHandle(hStdoutRead);
    ::CloseHandle(hStderrRead);

    result.pid = pi.dwProcessId;
    result.exitCode = (int)exitCode;
    result.stdoutText = std::move(stdoutText);
    result.stderrText = std::move(stderrText);
    result.succeeded = (exitCode == 0);
    result.cancelled = cancelled;
    if (freq.QuadPart != 0)
    {
        LARGE_INTEGER end{};
        ::QueryPerformanceCounter(&end);
        result.durationMs = (end.QuadPart - start.QuadPart) * 1000 / freq.QuadPart;
    }
    return result;
}

} // namespace

CaptureResult ExecEngine::runExternal(const std::wstring& exe,
                                      const std::wstring& cmdLine,
                                      const std::wstring& cwd,
                                      const std::wstring& envBlock)
{
    return launchAndCapture(exe, cmdLine, cwd, envBlock, nullptr);
}

CaptureResult ExecEngine::runPipeline2(const std::wstring& exe1, const std::wstring& cmd1,
                                       const std::wstring& exe2, const std::wstring& cmd2,
                                       const std::wstring& cwd, const std::wstring& envBlock)
{
    // Create a pipe for first->second.
    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!::CreatePipe(&hRead, &hWrite, &sa, 0))
    {
        return {};
    }
    ::SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    // Launch first command with stdout -> hWrite.
    STARTUPINFOW si1{};
    si1.cb = sizeof(si1);
    si1.dwFlags = STARTF_USESTDHANDLES;
    si1.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    si1.hStdOutput = hWrite;
    si1.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi1{};
    std::vector<wchar_t> cmd1Buf(cmd1.begin(), cmd1.end());
    cmd1Buf.push_back(L'\0');
    const wchar_t* cwdPtr = cwd.empty() ? nullptr : cwd.c_str();
    const wchar_t* envPtr = envBlock.empty() ? nullptr : envBlock.c_str();
    LPVOID envMutable = const_cast<LPVOID>(static_cast<const void*>(envPtr));

    const bool exe1HasPath = (exe1.find_first_of(L"\\/") != std::wstring::npos);
    const wchar_t* exe1Ptr = (!exe1.empty() && exe1HasPath) ? exe1.c_str() : nullptr;

    BOOL ok = ::CreateProcessW(exe1Ptr, cmd1Buf.data(), nullptr, nullptr, TRUE,
                               CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                               envMutable, cwdPtr, &si1, &pi1);
    ::CloseHandle(hWrite);
    if (!ok)
    {
        ::CloseHandle(hRead);
        return {};
    }
    ::CloseHandle(pi1.hThread);

    // Launch second command with stdin -> hRead.
    STARTUPINFOW si2{};
    si2.cb = sizeof(si2);
    si2.dwFlags = STARTF_USESTDHANDLES;
    si2.hStdInput = hRead;
    si2.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
    si2.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi2{};
    std::vector<wchar_t> cmd2Buf(cmd2.begin(), cmd2.end());
    cmd2Buf.push_back(L'\0');

    const bool exe2HasPath = (exe2.find_first_of(L"\\/") != std::wstring::npos);
    const wchar_t* exe2Ptr = (!exe2.empty() && exe2HasPath) ? exe2.c_str() : nullptr;

    ok = ::CreateProcessW(exe2Ptr, cmd2Buf.data(), nullptr, nullptr, TRUE,
                          CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                          envMutable, cwdPtr, &si2, &pi2);
    ::CloseHandle(hRead);
    if (!ok)
    {
        ::WaitForSingleObject(pi1.hProcess, 1000);
        ::CloseHandle(pi1.hProcess);
        return {};
    }
    ::CloseHandle(pi2.hThread);

    // Poll for the second process to exit instead of blocking forever so a
    // hung pipeline can still be interrupted with Esc / Ctrl+C.
    bool cancelled = false;
    DWORD waitedMs = 0;
    const DWORD kWaitLimitMs = 300000;
    for (;;)
    {
        const DWORD wait = ::WaitForSingleObject(pi2.hProcess, 50);
        if (wait == WAIT_OBJECT_0)
        {
            break;
        }
        if (wait == WAIT_TIMEOUT)
        {
            waitedMs += 50;
            if (g_cancelPoll && g_cancelPoll())
            {
                ::TerminateProcess(pi1.hProcess, 1);
                ::TerminateProcess(pi2.hProcess, 1);
                cancelled = true;
                break;
            }
            if (waitedMs >= kWaitLimitMs)
            {
                ::TerminateProcess(pi1.hProcess, 1);
                ::TerminateProcess(pi2.hProcess, 1);
                break;
            }
            continue;
        }
        break;
    }

    DWORD code1 = 0;
    DWORD code2 = 0;
    ::GetExitCodeProcess(pi1.hProcess, &code1);
    ::GetExitCodeProcess(pi2.hProcess, &code2);
    ::CloseHandle(pi1.hProcess);
    ::CloseHandle(pi2.hProcess);

    CaptureResult result;
    result.exitCode = (int)code2;
    result.pid = pi2.dwProcessId;
    result.succeeded = (code2 == 0);
    result.cancelled = cancelled;
    return result;
}

} // namespace kshell::ui

void kshell::ui::ExecEngine::setCancelPoll(CancelPoll poll)
{
    g_cancelPoll = poll;
}
