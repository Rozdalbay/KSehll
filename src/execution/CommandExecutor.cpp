#include "execution/CommandExecutor.h"

#include "utils/StringUtils.h"
#include "utils/PathUtils.h"
#include "utils/ErrorUtils.h"

#include <algorithm>

namespace kshell
{

namespace
{

std::wstring quoteArgument(const std::wstring& arg)
{
    if (arg.empty())
    {
        return L"\"\"";
    }

    bool hasSpace = arg.find_first_of(L" \t") != std::wstring::npos;
    bool hasQuote = arg.find(L'"') != std::wstring::npos;
    bool hasSpecial = arg.find_first_of(L"&|<>^") != std::wstring::npos;
    if (!hasSpace && !hasQuote && !hasSpecial)
    {
        return arg;
    }

    std::wstring quoted = L"\"";
    size_t backslash = 0;
    for (wchar_t c : arg)
    {
        if (c == L'\\')
        {
            ++backslash;
            continue;
        }
        if (c == L'"')
        {
            for (size_t i = 0; i < backslash; ++i)
            {
                quoted += L'\\';
            }
            quoted += L"\\\"";
            backslash = 0;
            continue;
        }
        for (size_t i = 0; i < backslash; ++i)
        {
            quoted += L'\\';
        }
        backslash = 0;
        quoted += c;
    }
    for (size_t i = 0; i < backslash; ++i)
    {
        quoted += L'\\';
    }
    quoted += L"\"";
    return quoted;
}

} // namespace

CommandExecutor::CommandExecutor(ExecutionContext context)
    : context_(std::move(context))
{
}

CommandExecutor::~CommandExecutor() = default;

bool CommandExecutor::hasRunningJob() const
{
    for (const auto& job : jobs_)
    {
        if (job.state == JobState::Running)
        {
            return true;
        }
    }
    return false;
}

const std::vector<Job>& CommandExecutor::getJobs() const
{
    return jobs_;
}

void CommandExecutor::updateJobStates()
{
    for (auto& job : jobs_)
    {
        if (job.state != JobState::Running)
        {
            continue;
        }
        bool anyRunning = false;
        for (const auto& proc : job.processes)
        {
            if (proc.proc.valid() && Process::isRunning(proc))
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

bool CommandExecutor::killJob(int jobId)
{
    for (auto& job : jobs_)
    {
        if (job.id == jobId)
        {
            bool killed = false;
            for (auto& proc : job.processes)
            {
                if (proc.proc.valid() && Process::terminate(proc))
                {
                    killed = true;
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

void CommandExecutor::reapJobs()
{
    updateJobStates();
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

std::optional<CommandExecutor::ResolvedCommand>
CommandExecutor::resolveCommand(const Command& command)
{
    ResolvedCommand resolved;
    resolved.executable = command.program;

    std::wstring lower = stringutils::toLower(command.program);

    std::wstring fullPath;
    if (pathutils::isAbsolutePath(command.program) ||
        (command.program.size() >= 2 && command.program[1] == L':'))
    {
        fullPath = command.program;
    }
    else if (command.program.find_first_of(L"\\/") != std::wstring::npos)
    {
        fullPath = pathutils::normalizePath(command.program);
    }
    else
    {
        std::vector<std::wstring> dirs;
        dirs.push_back(pathutils::getCurrentDirectory());
        for (const auto& dir : pathutils::getPathDirectories(context_.path))
        {
            if (!dir.empty())
                dirs.push_back(dir);
        }
        auto found = pathutils::searchExecutable(command.program, dirs);
        if (found)
        {
            fullPath = *found;
        }
    }

    if (fullPath.empty() || !pathutils::pathExists(fullPath))
    {
        return std::nullopt;
    }

    resolved.executable = fullPath;
    lower = stringutils::toLower(fullPath);
    resolved.isBatch = stringutils::endsWith(lower, L".bat") ||
                       stringutils::endsWith(lower, L".cmd");

    std::wstring cmdLine = quoteArgument(fullPath);
    for (const auto& arg : command.arguments)
    {
        cmdLine += L" " + quoteArgument(arg);
    }
    resolved.argsText = cmdLine;
    return resolved;
}

ProcessLaunchOptions CommandExecutor::buildLaunchOptions(const Command& command,
                                                         const ResolvedCommand& resolved,
                                                         const std::vector<RedirectionPlan>& redirects,
                                                         HANDLE defaultStdin, HANDLE defaultStdout,
                                                         HANDLE defaultStderr)
{
    ProcessLaunchOptions options;
    options.executable = resolved.executable;
    options.workingDirectory = context_.workingDirectory;
    options.environmentBlock = context_.environmentBlock;
    options.inheritHandles = true;

    if (resolved.isBatch)
    {
        std::wstring cmd = L"/c " + quoteArgument(resolved.executable);
        for (const auto& arg : command.arguments)
        {
            cmd += L" " + quoteArgument(arg);
        }
        options.executable = L"C:\\Windows\\System32\\cmd.exe";
        options.commandLine = cmd;
    }
    else
    {
        options.commandLine = resolved.argsText;
    }

    HANDLE hStdout = defaultStdout;
    HANDLE hStderr = defaultStderr;
    HANDLE hStdin = defaultStdin;
    WinHandle bothOutHandle;

    for (const auto& redirect : redirects)
    {
        switch (redirect.type)
        {
        case RedirectType::Input:
            hStdin = redirect.handle.valid() ? redirect.handle.get() : hStdin;
            break;
        case RedirectType::Output:
        case RedirectType::AppendOutput:
            hStdout = redirect.handle.valid() ? redirect.handle.get() : hStdout;
            break;
        case RedirectType::ErrorOutput:
        case RedirectType::ErrorAppend:
            hStderr = redirect.handle.valid() ? redirect.handle.get() : hStderr;
            break;
        case RedirectType::ErrorsToOutput:
            hStderr = hStdout;
            break;
        case RedirectType::BothOutput:
            hStderr = hStdout;
            break;
        }
    }

    options.stdinHandle.reset(hStdin);
    options.stdoutHandle.reset(hStdout);
    options.stderrHandle.reset(hStderr);
    options.inheritHandles = true;
    return options;
}

ExecutionResult CommandExecutor::runSingle(const Command& command, bool background)
{
    ExecutionResult result;

    auto resolved = resolveCommand(command);
    if (!resolved)
    {
        result.succeeded = false;
        result.exitCode = 1;
        return result;
    }

    auto redirectPlans = Redirection::prepare(command.redirects);
    if (!redirectPlans)
    {
        result.succeeded = false;
        result.exitCode = 1;
        return result;
    }

    HANDLE defaultStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE defaultStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE defaultStderr = GetStdHandle(STD_ERROR_HANDLE);

    auto options = buildLaunchOptions(command, *resolved, *redirectPlans,
                                      defaultStdin, defaultStdout, defaultStderr);

    auto process = Process::create(options);
    if (!process)
    {
        result.succeeded = false;
        result.exitCode = 1;
        return result;
    }

    result.succeeded = true;
    result.pid = process->pid;

    if (background)
    {
        int nextId = 1;
        for (const auto& j : jobs_)
        {
            if (j.id >= nextId)
            {
                nextId = j.id + 1;
            }
        }
        Job job;
        job.id = nextId;
        job.pid = process->pid;
        job.command = command.fullCommandLine();
        job.processes.push_back(std::move(*process));
        job.startTime = std::chrono::system_clock::now();
        job.state = JobState::Running;
        jobs_.push_back(std::move(job));
        result.jobId = nextId;
        return result;
    }

    auto waitResult = Process::wait(*process);
    result.exitCode = static_cast<int>(waitResult.exitCode);
    result.succeeded = waitResult.succeeded;
    return result;
}

ExecutionResult CommandExecutor::runPipe(const Pipeline& pipeline)
{
    ExecutionResult result;
    std::vector<RedirectionPlan> empty;

    if (pipeline.commands.size() == 1)
    {
        return runSingle(pipeline.commands[0], pipeline.background);
    }

    const size_t n = pipeline.commands.size();

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    std::vector<WinHandle> readPipes(n - 1);
    std::vector<WinHandle> writePipes(n - 1);
    for (size_t i = 0; i + 1 < n; ++i)
    {
        HANDLE r = nullptr;
        HANDLE w = nullptr;
        if (!::CreatePipe(&r, &w, &sa, 0))
        {
            result.succeeded = false;
            return result;
        }
        readPipes[i].reset(r);
        writePipes[i].reset(w);
    }

    std::vector<ProcessHandle> processes(n);

    for (size_t i = 0; i < n; ++i)
    {
        const Command& command = pipeline.commands[i];

        auto resolved = resolveCommand(command);
        if (!resolved)
        {
            result.succeeded = false;
            result.exitCode = 1;
            for (auto& p : processes)
            {
                if (p.proc.valid() && Process::isRunning(p))
                {
                    Process::terminate(p);
                }
            }
            return result;
        }

        auto redirectPlans = Redirection::prepare(command.redirects);
        if (!redirectPlans)
        {
            result.succeeded = false;
            result.exitCode = 1;
            return result;
        }

        HANDLE hStdin = (i == 0) ? GetStdHandle(STD_INPUT_HANDLE) : readPipes[i - 1].get();
        HANDLE hStdout = (i == n - 1) ? GetStdHandle(STD_OUTPUT_HANDLE) : writePipes[i].get();
        HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);

        for (const auto& redirect : *redirectPlans)
        {
            switch (redirect.type)
            {
            case RedirectType::Input:
                hStdin = redirect.handle.valid() ? redirect.handle.get() : hStdin;
                break;
            case RedirectType::Output:
            case RedirectType::AppendOutput:
                hStdout = redirect.handle.valid() ? redirect.handle.get() : hStdout;
                break;
            case RedirectType::ErrorOutput:
            case RedirectType::ErrorAppend:
                hStderr = redirect.handle.valid() ? redirect.handle.get() : hStderr;
                break;
            case RedirectType::ErrorsToOutput:
                hStderr = hStdout;
                break;
            case RedirectType::BothOutput:
                hStderr = hStdout;
                break;
            }
        }

        ProcessLaunchOptions options;
        options.executable = resolved->executable;
        options.workingDirectory = context_.workingDirectory;
        options.environmentBlock = context_.environmentBlock;
        options.inheritHandles = true;

        if (resolved->isBatch)
        {
            std::wstring cmd = L"/c " + quoteArgument(resolved->executable);
            for (const auto& arg : command.arguments)
            {
                cmd += L" " + quoteArgument(arg);
            }
            options.executable = L"C:\\Windows\\System32\\cmd.exe";
            options.commandLine = cmd;
        }
        else
        {
            options.commandLine = resolved->argsText;
        }

        options.stdinHandle.reset(hStdin);
        options.stdoutHandle.reset(hStdout);
        options.stderrHandle.reset(hStderr);

        auto proc = Process::create(options);
        if (!proc)
        {
            result.succeeded = false;
            result.exitCode = 1;
            for (auto& p : processes)
            {
                if (p.proc.valid() && Process::isRunning(p))
                {
                    Process::terminate(p);
                }
            }
            return result;
        }
        processes[i] = std::move(*proc);
    }

    for (auto& pipe : readPipes)
    {
        pipe.reset();
    }
    for (auto& pipe : writePipes)
    {
        pipe.reset();
    }

    result.succeeded = true;
    result.pid = processes.front().pid;

    if (pipeline.background)
    {
        int nextId = 1;
        for (const auto& j : jobs_)
        {
            if (j.id >= nextId)
            {
                nextId = j.id + 1;
            }
        }
        Job job;
        job.id = nextId;
        job.pid = processes.front().pid;
        job.command = pipeline.commands[0].fullCommandLine();
        job.processes = std::move(processes);
        job.startTime = std::chrono::system_clock::now();
        job.state = JobState::Running;
        jobs_.push_back(std::move(job));
        result.jobId = nextId;
        return result;
    }

    int exitCode = 0;
    for (size_t i = 0; i < processes.size(); ++i)
    {
        auto waitResult = Process::wait(processes[i]);
        if (i == processes.size() - 1)
        {
            exitCode = static_cast<int>(waitResult.exitCode);
        }
    }
    result.exitCode = exitCode;
    return result;
}

ExecutionResult CommandExecutor::executePipeline(const Pipeline& pipeline)
{
    return runPipe(pipeline);
}

} // namespace kshell
