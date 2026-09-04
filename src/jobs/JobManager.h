#pragma once

#ifndef KSHELL_JOBMANAGER_H
#define KSHELL_JOBMANAGER_H

#include <vector>
#include <optional>

#include "jobs/Job.h"
#include "execution/Process.h"

namespace kshell
{

class JobManager
{
public:
    int add(std::vector<ProcessHandle> processes, std::wstring command);

    void remove(int jobId);

    void updateStates();

    void reaper();

    bool kill(int jobId);

    bool isJobAlive(int jobId) const;

    std::optional<const Job*> findById(int jobId) const;

    const std::vector<Job>& getAll() const;
    const std::vector<Job>& getActive() const;

    int nextJobId() const;

private:
    std::vector<Job> jobs_;
    int nextId_ = 1;
};

} // namespace kshell

#endif // KSHELL_JOBMANAGER_H
