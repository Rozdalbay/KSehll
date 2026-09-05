#pragma once

#include <string>
#include <vector>

namespace kshell::git
{

// One file as reported by `git status --porcelain -vz`.
struct GitFileEntry
{
    std::wstring path;
    wchar_t      indexStatus = L' ';   // staging area status: (M/A/D/R/C/?/U/...)
    wchar_t      workTreeStatus = L' '; // working tree status
    bool         staged   = false;
    bool         untracked = false;
    bool         deleted  = false;
    // Short human-readable label: "modified", "added", "deleted", "untracked", "renamed".
    std::wstring label;
};

struct GitStatus
{
    bool          isRepo = false;
    std::wstring  branch;
    std::wstring  repoPath;
    std::wstring  workDir;
    std::wstring  remoteName;
    std::wstring  remoteUrl;
    std::wstring  lastCommitSubject;
    std::wstring  lastCommitShort;
    std::wstring  lastCommitAuthor;
    std::wstring  lastCommitDate;
    bool          detachedHead = false;
    int           ahead = 0;
    int           behind = 0;
    int           staged = 0;
    int           modified = 0;
    int           untracked = 0;
    int           deleted = 0;
    std::vector<std::wstring> changedFiles;   // legacy: flat list of paths
    std::vector<GitFileEntry> fileEntries;    // per-file detailed status
};

struct GitResult
{
    bool        ok = false;
    int         exitCode = 0;
    std::wstring stdoutText;
    std::wstring stderrText;
};

// A single diff line with an optional old/new line number.
struct GitDiffLine
{
    wchar_t   type = L' ';   // ' ' context, '+' added, '-' removed, '@' hunk, 'h' header
    int       oldNo = -1;
    int       newNo = -1;
    std::wstring text;
};

// One commit from `git log`.
struct GitCommit
{
    std::wstring hash;
    std::wstring shortHash;
    std::wstring subject;
    std::wstring body;         // description (may be empty)
    std::wstring author;
    std::wstring authorEmail;
    std::wstring date;          // ISO local date
    std::wstring relativeDate;  // e.g. "2 hours ago"
    std::wstring branches;      // refs pointing at this commit (--decorate)
    bool         isHead = false;
};

struct GitBranch
{
    std::wstring name;
    bool         isCurrent = false;
    bool         isRemote = false;
    std::wstring upstream;
    int          ahead = 0;
    int          behind = 0;
    std::wstring lastCommit;
};

struct GitRemote
{
    std::wstring name;
    std::wstring url;
    std::wstring fetchUrl;
};

struct GitStashEntry
{
    int          index = 0;
    std::wstring label;
    std::wstring message;
};

// Thin wrapper over the real `git` executable. Invokes git via CreateProcessW
// with piped stdout/stderr for capture. Locates git via PATH (same resolution
// the shell uses for external commands).
class Git
{
public:
    // Detect whether `dir` is inside a git work tree and gather a full status.
    GitStatus status(const std::wstring& dir);

    // Run an arbitrary git command in `dir` with `args`; returns captured output.
    GitResult run(const std::wstring& dir, const std::vector<std::wstring>& args);

    // Run git and return its raw output (used by the git builtin passthrough).
    GitResult runSimple(const std::wstring& dir, const std::wstring& argsLine);

    // Working-tree diff for one path (or the whole repo when path is empty).
    std::vector<GitDiffLine> diff(const std::wstring& dir, const std::wstring& path,
                                  bool cached);

    // Parse a `git diff`/`git show` text output into typed diff lines.
    static std::vector<GitDiffLine> parseDiff(const std::wstring& text);
    // Simple `git diff --stat` text (may be empty).
    std::wstring diffStat(const std::wstring& dir, bool cached);

    // Commit log.
    std::vector<GitCommit> log(const std::wstring& dir, int limit);

    // One commit's details (subject/body + diff/stat).
    GitResult show(const std::wstring& dir, const std::wstring& rev);
    // Full textual diff of a single commit (git show --no-color <rev>).
    std::wstring showText(const std::wstring& dir, const std::wstring& rev);

    // Branch lists.
    std::vector<GitBranch> localBranches(const std::wstring& dir);
    std::vector<GitBranch> remoteBranches(const std::wstring& dir);
    std::vector<GitBranch> allBranches(const std::wstring& dir);

    // Remotes.
    std::vector<GitRemote> remotes(const std::wstring& dir);

    // Stash list.
    std::vector<GitStashEntry> stashList(const std::wstring& dir);

    // Decorative commit graph text (git log --graph --oneline --all).
    std::wstring graphText(const std::wstring& dir, int limit);

    // Resolve the current branch name ("" when detached) and whether HEAD is
    // detached.
    std::wstring currentBranch(const std::wstring& dir);
    bool isDetachedHead(const std::wstring& dir);

    // The root of the repository containing `dir` ("" if not a repo).
    std::wstring findRepoRoot(const std::wstring& dir);

private:
    GitResult runImpl(const std::wstring& dir, const std::wstring& argsLine,
                      int timeoutMs = 30000, bool logOutput = true);
    // Whether `git` resolves on PATH.
    bool gitAvailable() const;
};

} // namespace kshell::git