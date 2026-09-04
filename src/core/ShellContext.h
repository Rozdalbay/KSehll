#pragma once

#ifndef KSHELL_SHELLCONTEXT_H
#define KSHELL_SHELLCONTEXT_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

#include "environment/Environment.h"
#include "config/Config.h"
#include "history/History.h"
#include "jobs/JobManager.h"
#include "execution/CommandExecutor.h"
#include "terminal/Console.h"
#include "terminal/IOutputSink.h"
#include "terminal/ConsoleSink.h"
#include "autocomplete/Autocomplete.h"
#include "utils/Logger.h"
#include "render/ThemeManager.h"
#include "core/VariableTracker.h"
#include "core/TraceLog.h"
#include <memory>

namespace kshell
{

class ShellContext
{
public:
    ShellContext();
    ~ShellContext();

    bool initialize();

    Environment& environment() { return environment_; }
    Config& config() { return config_; }
    History& history() { return history_; }
    JobManager& jobs() { return jobs_; }
    CommandExecutor& executor() { return *executor_; }
    Console& console() { return console_; }
    Logger& logger() { return logger_; }
    VariableTracker& variables() { return variables_; }
    TraceLog& trace() { return trace_; }
    const render::ThemeManager* themeManager_ = nullptr;

    // Route printed output/errors anywhere (console by default, or a session
    // scrollback buffer in the TUI). Panels/sessions that produce output set a
    // sink at construction time before any printing happens.
    void setOutputSink(IOutputSink* sink) { sink_ = sink; }
    IOutputSink* outputSink() const { return sink_; }

    // Optional hook invoked by the `theme` builtin so the UI can apply a theme
    // live. Unset in the classic REPL (theme just prints info then).
    std::function<void(const std::wstring&)> onThemeChange;

    // Optional pointer to the UI ThemeManager so the `theme` builtin can list
    // and reflect the active theme. Set by the TUI application at startup.
    void setThemeManager(const render::ThemeManager* tm) { themeManager_ = tm; }
    const render::ThemeManager* themeManager() const { return themeManager_; }

    std::wstring currentDirectory() const;
    bool setCurrentDirectory(const std::wstring& path);

    std::wstring promptText() const;

    void resetPromptDefaults();

    void printPrompt();
    void printError(const std::wstring& text);
    void printSuccess(const std::wstring& text);
    void printOutput(const std::wstring& text);

    bool requestExit() const { return exitRequested_; }
    void requestExit(bool value) { exitRequested_ = value; }
    int exitCode() const { return exitCode_; }
    void setExitCode(int code) { exitCode_ = code; }

    void refreshExecutor();

private:
    std::wstring buildEnvironmentBlock() const;

    Environment environment_;
    Config config_;
    History history_;
    JobManager jobs_;
    std::unique_ptr<CommandExecutor> executor_;
    Console console_;
    ConsoleSink consoleSink_{console_};
    IOutputSink* sink_ = nullptr;
    Logger logger_;
    VariableTracker variables_;
    TraceLog trace_;
    bool exitRequested_ = false;
    int exitCode_ = 0;
    bool initialized_ = false;
};

} // namespace kshell

#endif // KSHELL_SHELLCONTEXT_H
