#include "process/ProcessManager.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

namespace kshell::process
{

namespace
{

uint64_t filetimeToU64(const FILETIME& ft)
{
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

} // namespace

void ProcessManager::primeCpuSample()
{
    FILETIME idle{}, kernel{}, user{};
    ::GetSystemTimes(&idle, &kernel, &user);
    previousSystem_ = filetimeToU64(kernel) + filetimeToU64(user);
    previousIdle_ = filetimeToU64(idle);

    previous_.clear();
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        return;
    }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe))
    {
        do
        {
            FILETIME create{}, exit{}, kernel{}, user{};
            HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (h)
            {
                ::GetProcessTimes(h, &create, &exit, &kernel, &user);
                ::CloseHandle(h);
                previous_.push_back({pe.th32ProcessID, {filetimeToU64(kernel), filetimeToU64(user)}});
            }
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    primed_ = true;
}

std::vector<ProcessInfo> ProcessManager::snapshot()
{
    FILETIME idle{}, kernel{}, user{};
    ::GetSystemTimes(&idle, &kernel, &user);
    const uint64_t sysTotal = filetimeToU64(kernel) + filetimeToU64(user);
    const uint64_t sysIdle = filetimeToU64(idle);
    const uint64_t sysDelta = sysTotal > previousSystem_ ? (sysTotal - previousSystem_) : 0;

    std::vector<ProcessInfo> out;

    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        return out;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe))
    {
        do
        {
            ProcessInfo info;
            info.pid = pe.th32ProcessID;
            info.parentPid = pe.th32ParentProcessID;
            info.name = pe.szExeFile;
            info.threadCount = pe.cntThreads;

            HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                                     FALSE, pe.th32ProcessID);
            if (h)
            {
                FILETIME create{}, exit{}, k{}, u{};
                if (::GetProcessTimes(h, &create, &exit, &k, &u))
                {
                    const uint64_t kt = filetimeToU64(k);
                    const uint64_t ut = filetimeToU64(u);
                    if (primed_ && sysDelta > 0)
                    {
                        uint64_t prevK = 0, prevU = 0;
                        for (const auto& [pid, sample] : previous_)
                        {
                            if (pid == info.pid)
                            {
                                prevK = sample.kernel;
                                prevU = sample.user;
                                break;
                            }
                        }
                        const uint64_t procDelta = (kt + ut) - (prevK + prevU);
                        info.cpuPercent = 100.0 * static_cast<double>(procDelta) / static_cast<double>(sysDelta);
                        if (info.cpuPercent < 0.0)
                        {
                            info.cpuPercent = 0.0;
                        }
                        if (info.cpuPercent > 100.0)
                        {
                            info.cpuPercent = 100.0;
                        }
                    }
                }

                PROCESS_MEMORY_COUNTERS_EX pmc{};
                if (::GetProcessMemoryInfo(h, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
                {
                    info.workingSetBytes = static_cast<int64_t>(pmc.WorkingSetSize);
                }

                wchar_t path[MAX_PATH] = {0};
                DWORD n = MAX_PATH;
                if (::QueryFullProcessImageNameW(h, 0, path, &n))
                {
                    info.exePath = path;
                }
                ::CloseHandle(h);
            }

            out.push_back(std::move(info));
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);

    // Save current CPU sample for the next call.
    previousSystem_ = sysTotal;
    previousIdle_ = sysIdle;
    previous_.clear();
    for (const auto& info : out)
    {
        HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, info.pid);
        if (h)
        {
            FILETIME create{}, exit{}, k{}, u{};
            if (::GetProcessTimes(h, &create, &exit, &k, &u))
            {
                previous_.push_back({info.pid, {filetimeToU64(k), filetimeToU64(u)}});
            }
            ::CloseHandle(h);
        }
    }
    primed_ = true;

    return out;
}

bool ProcessManager::queryExePath(uint32_t pid, std::wstring& path)
{
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
    {
        return false;
    }
    wchar_t buf[MAX_PATH] = {0};
    DWORD n = MAX_PATH;
    bool ok = ::QueryFullProcessImageNameW(h, 0, buf, &n) != FALSE;
    ::CloseHandle(h);
    if (ok)
    {
        path = buf;
    }
    return ok;
}

bool ProcessManager::terminate(uint32_t pid, TerminateAction action)
{
    if (action == TerminateAction::Force)
    {
        HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!h)
        {
            return false;
        }
        BOOL ok = ::TerminateProcess(h, 1);
        ::CloseHandle(h);
        return ok != FALSE;
    }

    // Graceful: send WM_CLOSE to the process's main windows.
    struct Ctx
    {
        DWORD pid;
        bool  found;
    };
    Ctx ctx{pid, false};
    ::EnumWindows([](HWND hwnd, LPARAM lparam) -> BOOL {
        Ctx* c = reinterpret_cast<Ctx*>(lparam);
        DWORD pid = 0;
        ::GetWindowThreadProcessId(hwnd, &pid);
        if (pid == c->pid)
        {
            ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
            c->found = true;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.found)
    {
        return true;
    }
    // No window: attempt TerminateProcess as fallback.
    return terminate(pid, TerminateAction::Force);
}

bool ProcessManager::spawn(const std::wstring& commandLine, std::wstring* errorText)
{
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = commandLine;
    std::vector<wchar_t> buf(mutableCmd.begin(), mutableCmd.end());
    buf.push_back(L'\0');
    if (!::CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
                          CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                          nullptr, nullptr, &si, &pi))
    {
        if (errorText)
        {
            *errorText = L"Failed to launch (error " + std::to_wstring(::GetLastError()) + L")";
        }
        return false;
    }
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
}

} // namespace kshell::process
