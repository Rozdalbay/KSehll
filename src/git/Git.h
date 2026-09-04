#pragma once

#include <string>
#include <vector>

namespace kshell::git
{

struct GitStatus
{
    bool          isRepo = false;
    std::wstring  branch;
    std::wstring  repoPath;
    int           ahead = 0;
    int           behind = 0;
    int           staged = 0;
    int           modified = 0;
    int           untracked = 0;
    std::vector<std::wstring> changedFiles;
};

struct GitResult
{
    bool        ok = false;
    int         exitCode = 0;
    std::wstring stdoutText;
    std::wstring stderrText;
};

// Thin wrapper over the real `git` executable. Invokes git via CreateProcessW
// with piped stdout/stderr for capture. Locates git via PATH (same resolution
// the shell uses for external commands).
class Git
{
public:
    // Detect whether `dir` is inside a git work tree and gather a light status.
    GitStatus status(const std::wstring& dir);

    // Run an arbitrary git command in `dir` with `args`; returns captured output.
    GitResult run(const std::wstring& dir, const std::vector<std::wstring>& args);

    // Run git and return its raw output (used by the git builtin passthrough).
    GitResult runSimple(const std::wstring& dir, const std::wstring& argsLine);

private:
    GitResult runImpl(const std::wstring& dir, const std::wstring& argsLine);
    // Whether `git` resolves on PATH.
    bool gitAvailable() const;
};

} // namespace kshell::git
