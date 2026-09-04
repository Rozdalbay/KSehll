#pragma once

#ifndef KSHELL_COMMANDEXECUTOR_H
#define KSHELL_COMMANDEXECUTOR_H

#include <string>
#include <vector>
#include <optional>

#include "parser/Command.h"
#include "execution/Process.h"
#include "execution/Redirection.h"
#include "jobs/Job.h"

namespace kshell
{

struct ExecutionContext
{
    std::wstring workingDirectory;
    std::wstring environmentBlock;
    std::wstring path;
    std::wstring prompt;
    bool isBatchMode = false;
};

struct ExecutionResult
{
    bool succeeded = false;
    int exitCode = 0;
    int jobId = -1;
    DWORD pid = 0;
};

class CommandExecutor
{
public:
    explicit CommandExecutor(ExecutionContext context);
    ~CommandExecutor();

    ExecutionResult executePipeline(const Pipeline& pipeline);

    const std::vector<Job>& getJobs() const;
    void updateJobStates();
    bool killJob(int jobId);
    void reapJobs();

    bool hasRunningJob() const;

private:
    struct ResolvedCommand
    {
        std::wstring executable;
        std::wstring argsText;
        bool isBatch = false;
    };

    std::optional<ResolvedCommand> resolveCommand(const Command& command);

    ExecutionResult runPipe(const Pipeline& pipeline);
    ExecutionResult runSingle(const Command& command, bool background);

    ProcessLaunchOptions buildLaunchOptions(const Command& command,
                                            const ResolvedCommand& resolved,
                                            const std::vector<RedirectionPlan>& redirects,
                                            HANDLE defaultStdin, HANDLE defaultStdout,
                                            HANDLE defaultStderr);

    ExecutionContext context_;
    std::vector<Job> jobs_;
};

} // namespace kshell

#endif // KSHELL_COMMANDEXECUTOR_H
