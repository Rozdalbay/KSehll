#include "core/Shell.h"
#include "builtin/BuiltinCommand.h"
#include "builtin/BuiltinRegistry.h"
#include "utils/StringUtils.h"
#include "utils/PathUtils.h"
#include "utils/ErrorUtils.h"
#include "parser/Lexer.h"
#include "parser/Parser.h"

#include <windows.h>

#include <iostream>
#include <algorithm>

namespace kshell
{

namespace
{

BOOL WINAPI ignoreCtrlC(DWORD ctrlType)
{
    // Return TRUE to indicate we handled it: prevents the default handler from
    // terminating the shell. Foreground children sharing the console still
    // receive the signal and are interrupted by it.
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT)
    {
        return TRUE;
    }
    return FALSE;
}

std::vector<BuiltinEntry> builtinTable()
{
    std::vector<BuiltinEntry> entries;
    for (const auto& reg : builtinRegistry())
    {
        entries.push_back({reg.name.c_str(), reg.function});
    }
    return entries;
}

} // namespace

Shell::Shell()
{
}

Shell::~Shell()
{
    shutdown();
}

bool Shell::initialize()
{
    if (!terminal_.initialize())
    {
        return false;
    }

    if (!ctx_.initialize())
    {
        return false;
    }

    std::vector<std::wstring> builtinNames;
    for (const auto& entry : builtinTable())
    {
        builtinNames.push_back(entry.name);
    }
    autocomplete_.setBuiltinNames(builtinNames);

    const auto aliases = ctx_.config().getAliases();
    std::vector<std::wstring> aliasNames;
    for (const auto& [name, _] : aliases)
    {
        (void)_;
        aliasNames.push_back(name);
    }

    return true;
}

void Shell::shutdown()
{
    if (!ctx_.requestExit())
    {
        ctx_.history().saveToFile(ctx_.config().getHistoryFilePath());
    }
}

std::vector<std::wstring> Shell::resolveAliases(const std::vector<std::wstring>& tokens)
{
    if (tokens.empty())
    {
        return tokens;
    }

    const auto aliases = ctx_.config().getAliases();
    const auto it = aliases.find(tokens[0]);
    if (it == aliases.end())
    {
        return tokens;
    }

    const std::wstring aliasCmd = it->second;
    auto expanded = stringutils::splitCommandLine(aliasCmd);
    if (expanded.empty())
    {
        return tokens;
    }

    for (size_t i = 1; i < tokens.size(); ++i)
    {
        expanded.push_back(tokens[i]);
    }
    return expanded;
}

bool Shell::expandVariables(std::wstring& input)
{
    ctx_.environment().refresh();
    input = ctx_.environment().expand(input);
    return true;
}

Shell::ProcessInputResult Shell::processLine(const std::wstring& line)
{
    const std::wstring trimmed = stringutils::trim(line);
    if (trimmed.empty())
    {
        return ProcessInputResult::Continue;
    }

    std::wstring expanded = trimmed;
    expandVariables(expanded);

    ctx_.history().add(trimmed);

    std::optional<ParseError> error;
    Lexer lexer(expanded);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto pipelines = parser.parse(error);

    if (error)
    {
        ctx_.printError(L"Parse error: " + error->message);
        return ProcessInputResult::Continue;
    }

    for (auto& pipeline : pipelines)
    {
        for (auto& command : pipeline.commands)
        {
            auto aliasTokens = resolveAliases(
                [&]() -> std::vector<std::wstring> {
                    std::vector<std::wstring> result;
                    result.push_back(command.program);
                    for (const auto& arg : command.arguments)
                    {
                        result.push_back(arg);
                    }
                    return result;
                }());

            if (!aliasTokens.empty())
            {
                command.program = aliasTokens[0];
                command.arguments.clear();
                for (size_t i = 1; i < aliasTokens.size(); ++i)
                {
                    command.arguments.push_back(aliasTokens[i]);
                }
            }
        }
    }

    for (auto& pipeline : pipelines)
    {
        bool builtinHandled = false;

        for (const auto& cmd : pipeline.commands)
        {
            BuiltinFunction builtinFunc = nullptr;
            for (const auto& entry : builtinTable())
            {
                if (stringutils::equalsIgnoreCase(cmd.program, entry.name))
                {
                    builtinFunc = entry.function;
                    break;
                }
            }

            if (builtinFunc && pipeline.commands.size() == 1)
            {
                std::vector<std::wstring> fullArgs;
                fullArgs.push_back(cmd.program);
                for (const auto& arg : cmd.arguments)
                {
                    fullArgs.push_back(arg);
                }
                auto result = builtinFunc(ctx_, fullArgs);
                if (result.exitRequested)
                {
                    return ProcessInputResult::Exit;
                }
                // A builtin may report handled == false to request fall-through
                // to external execution (e.g. the `git` passthrough builtin).
                if (result.handled)
                {
                    builtinHandled = true;
                }
            }
        }

        if (builtinHandled)
        {
            continue;
        }

        ctx_.refreshExecutor();
        auto result = ctx_.executor().executePipeline(pipeline);

        if (result.succeeded && result.pid != 0 && !pipeline.background)
        {
            // process finished, exit code is ready
        }
        else if (!result.succeeded && !pipeline.background)
        {
            ctx_.printError(L"Command failed: " + pipeline.commands[0].program);
        }
    }

    return ProcessInputResult::Continue;
}

void Shell::printBanner()
{
    ctx_.console().setColor(ctx_.config().colors.promptColor);
    terminal_.printLine(L"KShell " KSHELL_VERSION);
    ctx_.console().resetColor();
    terminal_.printLine(L"Type \"help\" for available commands.");
    terminal_.printLine(L"");
}

int Shell::run()
{
    // The shell must survive Ctrl+C; child processes sharing the console are
    // signalled instead, which lets us interrupt a running foreground process
    // without terminating the shell itself.
    if (!::SetConsoleCtrlHandler(ignoreCtrlC, TRUE))
    {
        ctx_.printError(L"Failed to install console control handler");
    }

    printBanner();

    std::vector<std::wstring> builtinNames;
    for (const auto& entry : builtinTable())
    {
        builtinNames.push_back(entry.name);
    }

    while (!ctx_.requestExit())
    {
        ctx_.executor().reapJobs();

        const std::wstring prompt = ctx_.promptText();
        auto pathDirs = pathutils::getPathDirectories(ctx_.environment().getPath());

        const auto aliases = ctx_.config().getAliases();
        std::vector<std::wstring> aliasNames;
        for (const auto& [name, _] : aliases)
        {
            aliasNames.push_back(name);
        }

        auto inputResult = terminal_.input().readLine(
            prompt,
            ctx_.history(),
            autocomplete_,
            ctx_.config(),
            ctx_.currentDirectory(),
            pathDirs,
            aliasNames,
            builtinNames
        );

        if (inputResult.exitRequested)
        {
            break;
        }

        if (inputResult.cancelled)
        {
            terminal_.printLine(L"");
            continue;
        }

        auto result = processLine(inputResult.line);
        if (result == ProcessInputResult::Exit)
        {
            break;
        }
    }

    ::SetConsoleCtrlHandler(ignoreCtrlC, FALSE);
    ctx_.history().saveToFile(ctx_.config().getHistoryFilePath());
    return ctx_.exitCode();
}

} // namespace kshell