#include "system/SystemMonitor.h"

#include <windows.h>
#include <tlhelp32.h>

namespace kshell::system
{

namespace
{

uint64_t filetimeToU64(const FILETIME& ft)
{
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

uint64_t getUptimeSeconds()
{
    return static_cast<uint64_t>(::GetTickCount64() / 1000u);
}

} // namespace

SystemMonitor::SystemMonitor() = default;

double SystemMonitor::cpuRatio()
{
    FILETIME idle{}, kernel{}, user{};
    ::GetSystemTimes(&idle, &kernel, &user);
    const uint64_t sys = filetimeToU64(kernel) + filetimeToU64(user);
    const uint64_t idl = filetimeToU64(idle);
    double ratio = 0.0;
    if (primed_ && sys > previousSystem_)
    {
        const uint64_t sysDelta = sys - previousSystem_;
        const uint64_t idleDelta = idl > previousIdle_ ? (idl - previousIdle_) : 0;
        const uint64_t usedDelta = sysDelta > idleDelta ? (sysDelta - idleDelta) : 0;
        ratio = static_cast<double>(usedDelta) / static_cast<double>(sysDelta);
    }
    previousSystem_ = sys;
    previousIdle_ = idl;
    primed_ = true;
    if (ratio < 0.0)
    {
        ratio = 0.0;
    }
    if (ratio > 1.0)
    {
        ratio = 1.0;
    }
    return ratio * 100.0;
}

SystemStats SystemMonitor::refresh()
{
    SystemStats s;
    s.cpuPercent = cpuRatio();

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (::GlobalMemoryStatusEx(&mem))
    {
        s.ramTotalBytes = mem.ullTotalPhys;
        s.ramUsedBytes = mem.ullTotalPhys - mem.ullAvailPhys;
        s.ramPercent = mem.dwMemoryLoad;
    }

    // Disk usage of the drive containing the CWD.
    wchar_t cwd[MAX_PATH] = {0};
    if (::GetCurrentDirectoryW(MAX_PATH, cwd))
    {
        ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFree{};
        if (::GetDiskFreeSpaceExW(cwd, &freeBytes, &totalBytes, &totalFree))
        {
            s.diskFreeBytes = freeBytes.QuadPart;
            s.diskTotalBytes = totalBytes.QuadPart;
            if (totalBytes.QuadPart > 0)
            {
                s.diskPercent = 100.0 * (1.0 - static_cast<double>(freeBytes.QuadPart) / static_cast<double>(totalBytes.QuadPart));
            }
        }
    }

    uint32_t procCount = 0;
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (::Process32FirstW(snap, &pe))
        {
            do
            {
                ++procCount;
            } while (::Process32NextW(snap, &pe));
        }
        ::CloseHandle(snap);
    }
    s.processCount = procCount;
    s.uptimeSeconds = getUptimeSeconds();

    s.cpuHistory = cpuHistory_;
    s.ramHistory = ramHistory_;
    s.cpuHistory.push_back(s.cpuPercent);
    s.ramHistory.push_back(s.ramPercent);
    const size_t kHistory = 60;
    if (s.cpuHistory.size() > kHistory)
    {
        s.cpuHistory.erase(s.cpuHistory.begin());
    }
    if (s.ramHistory.size() > kHistory)
    {
        s.ramHistory.erase(s.ramHistory.begin());
    }
    cpuHistory_ = s.cpuHistory;
    ramHistory_ = s.ramHistory;

    return s;
}

} // namespace kshell::system
