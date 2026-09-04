#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kshell::process
{

struct ProcessInfo
{
    uint32_t      pid = 0;
    uint32_t      parentPid = 0;
    std::wstring  name;
    std::wstring  exePath;
    int64_t       workingSetBytes = 0; // RAM in bytes
    double        cpuPercent = 0.0;
    uint32_t      threadCount = 0;
    wchar_t       status = L' ';
};

enum class TerminateAction
{
    Request, // WM_CLOSE / normal terminate
    Force    // TerminateProcess
};

// Enumerates and inspects Windows processes v4 Toolhelp32. Lightweight; no
// polling loops on the UI thread (call via refresh()).
class ProcessManager
{
public:
    // Full re-enumeration of the process table.
    std::vector<ProcessInfo> snapshot();

    // CPU time map used to compute per-process CPU % between samples.
    void primeCpuSample();

    // Terminate a process. Safe by default (requires confirmation at UI layer).
    bool terminate(uint32_t pid, TerminateAction action);

    // Find the executable path of a process.
    bool queryExePath(uint32_t pid, std::wstring& path);

    // Launch a command line via CreateProcessW (detached, own window hidden).
    // Returns false on failure; S_OK style error code in errorCode.
    bool spawn(const std::wstring& commandLine, std::wstring* errorText);

private:
    struct CpuSample
    {
        uint64_t kernel;
        uint64_t user;
    };
    std::vector<std::pair<uint32_t, CpuSample>> previous_;
    uint64_t previousSystem_ = 0;
    uint64_t previousIdle_ = 0;
    bool     primed_ = false;
};

} // namespace kshell::process
