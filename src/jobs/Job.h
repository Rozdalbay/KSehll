#pragma once

#ifndef KSHELL_JOB_H
#define KSHELL_JOB_H

#include <windows.h>

#include <string>
#include <vector>
#include <chrono>

#include "utils/WinHandle.h"
#include "execution/Process.h"

namespace kshell
{

enum class JobState
{
    Running,
    Finished,
    Failed,
    Terminated
};

struct Job
{
    int id = 0;
    DWORD pid = 0;
    std::wstring command;
    std::vector<ProcessHandle> processes;
    std::chrono::system_clock::time_point startTime;
    JobState state = JobState::Running;
    DWORD exitCode = 0;
};

} // namespace kshell

#endif // KSHELL_JOB_H
