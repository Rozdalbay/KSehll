#pragma once

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

#include "ui/Ui.h"
#include "execution/Process.h"

namespace kshell::ui
{

struct CaptureResult
{
    int         exitCode = 0;
    std::wstring stdoutText;
    std::wstring stderrText;
    DWORD       pid = 0;
    bool        succeeded = false;
    bool        cancelled = false;
    long long   durationMs = 0;
};

// Poll callback invoked periodically while an external command runs.
// Returning true requests cancellation; the child process is terminated so
// the shell stays responsive even if the command hangs (e.g. git waiting on
// the network, a credential prompt, or an editor).
using CancelPoll = bool (*)();

// Run a command in a given directory with stdout/stderr piped and captured.
// Supports working directory and environment block. This is used by the TUI
// shell pane so external program output goes into the scrollback buffer
// rather than directly to the console.
class ExecEngine
{
public:
    // Install/uninstall a cancellation poll. The TUI sets one before running
    // commands so a hung process can be interrupted with Esc / Ctrl+C.
    static void setCancelPoll(CancelPoll poll);

    // Run a single external command and capture all output.
    static CaptureResult runExternal(const std::wstring& exe,
                                     const std::wstring& cmdLine,
                                     const std::wstring& cwd,
                                     const std::wstring& envBlock);

    // Run a pipeline of 2 commands (first's stdout -> second's stdin).
    static CaptureResult runPipeline2(const std::wstring& exe1, const std::wstring& cmd1,
                                      const std::wstring& exe2, const std::wstring& cmd2,
                                      const std::wstring& cwd, const std::wstring& envBlock);
};

} // namespace kshell::ui
