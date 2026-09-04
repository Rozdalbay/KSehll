#pragma once

#ifndef KSHELL_PROCESS_H
#define KSHELL_PROCESS_H

#include <windows.h>

#include <string>
#include <optional>

#include "utils/WinHandle.h"

namespace kshell
{

struct ProcessHandle
{
    WinHandle proc;
    WinHandle thread;
    DWORD pid = 0;
};

struct ProcessResult
{
    DWORD exitCode = 0;
    bool succeeded = false;
};

struct ProcessLaunchOptions
{
    std::wstring executable;
    std::wstring commandLine;
    std::wstring workingDirectory;
    std::wstring environmentBlock;
    WinHandle stdinHandle;
    WinHandle stdoutHandle;
    WinHandle stderrHandle;
    bool inheritHandles = true;
};

class Process
{
public:
    static std::optional<ProcessHandle> create(const ProcessLaunchOptions& options);

    static ProcessResult wait(const ProcessHandle& handle, DWORD timeoutMs = INFINITE);

    static bool isRunning(const ProcessHandle& handle);

    static bool terminate(const ProcessHandle& handle, DWORD exitCode = 1);

    static bool isProcessAlive(DWORD pid);

    static bool killByPid(DWORD pid);
};

} // namespace kshell

#endif // KSHELL_PROCESS_H
