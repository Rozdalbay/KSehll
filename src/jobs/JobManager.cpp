#include "jobs/JobManager.h"

#include <algorithm>

namespace kshell
{

int JobManager::add(std::vector<ProcessHandle> processes, std::wstring command)
{
    Job job;
    job.id = nextJobId();
    job.pid = processes.empty() ? 0 : processes.front().pid;
    job.command = std::move(command);
    job.processes = std::move(processes);
    job.startTime = std::chrono::system_clock::now();
    job.state = JobState::Running;
    job.exitCode = 0;

    jobs_.push_back(std::move(job));
    ++nextId_;
    return job.id;
}

void JobManager::remove(int jobId)
{
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                               [jobId](const Job& j) { return j.id == jobId; }),
                jobs_.end());
}

void JobManager::updateStates()
{
    for (auto& job : jobs_)
    {
        if (job.state == JobState::Running)
        {
            if (job.processes.empty())
            {
                job.state = JobState::Finished;
                continue;
            }
            bool anyRunning = false;
            for (const auto& proc : job.processes)
            {
                if (!proc.proc.valid())
                {
                    continue;
                }
                if (Process::isRunning(proc))
                {
                    anyRunning = true;
                    break;
                }
            }
            if (!anyRunning)
            {
                DWORD code = 0;
                if (!job.processes.empty() && job.processes.front().proc.valid())
                {
                    ::GetExitCodeProcess(job.processes.front().proc.get(), &code);
                }
                job.exitCode = code;
                job.state = (code == 0) ? JobState::Finished : JobState::Failed;
            }
        }
    }
}

void JobManager::reaper()
{
    updateStates();

    const auto now = std::chrono::system_clock::now();
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                               [now](const Job& j)
                               {
                                   if (j.state == JobState::Finished ||
                                       j.state == JobState::Failed ||
                                       j.state == JobState::Terminated)
                                   {
                                       const auto elapsed = std::chrono::duration_cast<
                                           std::chrono::seconds>(now - j.startTime);
                                       return elapsed.count() > 60;
                                   }
                                   return false;
                               }),
                jobs_.end());
}

bool JobManager::kill(int jobId)
{
    for (auto& job : jobs_)
    {
        if (job.id == jobId)
        {
            bool killed = false;
            for (auto& proc : job.processes)
            {
                if (proc.proc.valid())
                {
                    if (Process::terminate(proc))
                    {
                        killed = true;
                    }
                }
            }
            if (killed)
            {
                job.state = JobState::Terminated;
            }
            return killed;
        }
    }
    return false;
}

bool JobManager::isJobAlive(int jobId) const
{
    for (const auto& job : jobs_)
    {
        if (job.id == jobId)
        {
            return job.state == JobState::Running;
        }
    }
    return false;
}

std::optional<const Job*> JobManager::findById(int jobId) const
{
    for (const auto& job : jobs_)
    {
        if (job.id == jobId)
        {
            return &job;
        }
    }
    return std::nullopt;
}

const std::vector<Job>& JobManager::getAll() const
{
    return jobs_;
}

const std::vector<Job>& JobManager::getActive() const
{
    // Filtered view is not cached; return jobs_ (callers should check state).
    return jobs_;
}

int JobManager::nextJobId() const
{
    int maxId = 0;
    for (const auto& job : jobs_)
    {
        if (job.id > maxId)
        {
            maxId = job.id;
        }
    }
    return maxId + 1;
}

} // namespace kshell
