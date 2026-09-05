#pragma once

#include "ui/Ui.h"
#include "ui/Key.h"
#include "system/SystemMonitor.h"
#include "git/Git.h"
#include "git/GitHub.h"

#include <atomic>
#include <memory>
#include <thread>

namespace kshell
{
class JobManager;
class History;
class Environment;
class VariableTracker;
class TraceLog;
namespace system { class SystemMonitor; }
} // namespace kshell

namespace kshell::ui
{

// File Manager panel: browse directories, list files.
class FilePane : public Pane
{
public:
    FilePane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    bool onMouseClick(int rowInPane, int colInPane, bool doubleClick) override;
    void refresh() override;

    void navigateTo(const std::wstring& dir);

private:
    void                openSelected();
    void                toggleHidden();
    struct FileEntry
    {
        std::wstring name;
        bool         isDir;
    };
    std::wstring currentDir_;
    std::vector<FileEntry> entries_;
    bool showHidden_ = false;
    int scrollOffset_ = 0;
    int selectedIdx_ = 0;
    std::wstring filter_;
};

// Process Manager panel.
class ProcessPane : public Pane
{
public:
    ProcessPane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    bool onMouseClick(int rowInPane, int colInPane, bool doubleClick) override;
    void refresh() override;

private:
    struct ProcEntry
    {
        uint32_t pid;
        uint32_t parentPid;
        std::wstring name;
        double cpu;
        int64_t ram;
        int depth; // tree nesting depth when in tree mode
    };
    void rebuildTree();

    std::vector<ProcEntry> procs_;
    int scrollOffset_ = 0;
    int selectedIdx_ = 0;
    std::wstring filter_;
    bool needsRefresh_ = true;
    bool treeMode_ = true;
    unsigned long long lastRefreshTime_ = 0;
};

// System Monitor panel.
class SystemPane : public Pane
{
public:
    SystemPane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    void refresh() override;

private:
    // Persistent monitor so CPU sampling primes across refresh() calls and
    // history accumulates into a continuous real-time graph.
    system::SystemMonitor monitor_;

    double cpu_ = 0;
    double ram_ = 0;
    uint64_t ramUsed_ = 0;
    uint64_t ramTotal_ = 0;
    double disk_ = 0;
    uint64_t diskFree_ = 0;
    uint64_t diskTotal_ = 0;
    uint64_t procCount_ = 0;
    uint64_t uptime_ = 0;
    std::vector<double> cpuHistory_;
    std::vector<double> ramHistory_;
    bool needsRefresh_ = true;
};
class GitPane : public Pane
{
public:
    GitPane();
    ~GitPane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    bool onMouseClick(int rowInPane, int colInPane, bool doubleClick) override;
    void refresh() override;

    void setWorkDir(const std::wstring& dir);
    bool operationActive() const { return op_ != nullptr; }

    // Read-only accessors for tests and shell integrations.
    bool isRepo() const { return isRepo_; }
    int  section() const { return static_cast<int>(section_); }
    int  fileCount() const { return (int)files_.size(); }
    int  branchCount() const { return (int)branches_.size(); }
    int  commitCount() const { return (int)commits_.size(); }
    int  remoteCount() const { return (int)remotes_.size(); }
    int  statusStaged() const { return status_.staged; }
    int  statusModified() const { return status_.modified; }
    const std::wstring& branchName() const { return status_.branch; }
    const std::wstring& diffFile() const { return diffFile_; }

private:
    enum class Section { Overview, Changes, Branches, History, Graph, Remotes, GitHub };
    enum class DialogKind { None, Confirm, NewBranch, DeleteBranch, RenameBranch,
                            MergeBranch, Stash, PopStash, CloneRepo, AddRemote,
                            RemoveRemote, GithubToken, CreatePR, CreateIssue };
    enum class Focus { Files, Diff, CommitMsg, CommitDesc, Buttons, None };

    // A background operation runs in its own thread so the UI never blocks.
    struct AsyncOp
    {
        Section section = Section::Overview;
        std::wstring label;
        std::wstring workDir;
        bool github = false;                 // run via github::GitHub, not git
        std::wstring ghRepo;                 // "owner/repo" for GitHub calls
        std::wstring ghAction;               // "user" | "repos" | "pulls" | "issues" | "createPR" | "createIssue"
        std::wstring ghExtra1, ghExtra2, ghExtra3;
        bool ghPrs = false;

        std::vector<std::wstring> args;      // git arguments
        std::wstring extraPath;              // file path for diff/discard ops
        std::wstring extraA, extraB, extraC; // generic params

        std::atomic<bool> finished{false};
        std::thread       worker;

        // Result fields (written by the worker before finished=true; the pane
        // only reads them after the worker finished, so no data race).
        git::GitResult            result;
        std::vector<git::GitDiffLine> diffLines;
        github::GitHubUser        ghUser;
        std::vector<github::GitHubRepo>  ghRepos;
        std::vector<github::GitHubPR>    ghPRs;
        std::vector<github::GitHubIssue> ghIssues;
        std::wstring              ghOut;     // created url / status text
        std::wstring              ghError;
        bool                      ok = false;
    };

    // ---- git engine state ------------------------------------------------
    std::wstring  workDir_;
    bool          isRepo_ = false;
    git::Git      git_;
    git::GitStatus status_;
    unsigned long long lastStatusTime_ = 0;

    // ---- navigation ------------------------------------------------------
    Section section_ = Section::Overview;
    Focus   focus_ = Focus::Files;
    std::wstring notice_;      // transient message shown in the hints bar
    unsigned long long noticeTime_ = 0;

    // ---- changes / diff --------------------------------------------------
    std::vector<git::GitFileEntry> files_;
    int selectedFile_ = 0;
    int fileScroll_ = 0;
    std::vector<git::GitDiffLine> diff_;
    std::wstring diffFile_;    // path the cached diff belongs to
    bool  diffIsCached_ = false;
    int   diffScroll_ = 0;
    bool  sideBySide_ = false;
    bool  diffLoading_ = false;
    std::wstring commitMsg_, commitDesc_;
    int   commitCursor_ = 0;

    // ---- branches --------------------------------------------------------
    std::vector<git::GitBranch> branches_;
    int selectedBranch_ = 0;
    int branchScroll_ = 0;

    // ---- history ---------------------------------------------------------
    std::vector<git::GitCommit> commits_;
    int selectedCommit_ = 0;
    int historyScroll_ = 0;
    std::wstring commitDetail_;   // show --stat for selected commit
    std::vector<git::GitDiffLine> commitDiff_;
    bool commitDetailLoading_ = false;

    // ---- graph -----------------------------------------------------------
    std::wstring graph_;
    int graphScroll_ = 0;

    // ---- remotes ---------------------------------------------------------
    std::vector<git::GitRemote> remotes_;
    int selectedRemote_ = 0;
    int remoteScroll_ = 0;

    // ---- github ----------------------------------------------------------
    int ghSelected_ = 0;
    int ghScroll_ = 0;
    github::GitHubUser         ghUser_;
    std::vector<github::GitHubRepo>  ghRepos_;
    std::vector<github::GitHubPR>    ghPRs_;
    std::vector<github::GitHubIssue> ghIssues_;
    bool  ghConnected_ = false;
    std::wstring ghAccountInfo_;
    std::wstring ghRepoSelected_;
    int  ghRepoIdx_ = -1;
    bool ghLoading_ = false;
    std::wstring ghToken_;      // cached copy for new connections

    // ---- async op management --------------------------------------------
    std::unique_ptr<AsyncOp> op_;
    std::vector<std::wstring> lastOpLog_;   // rendered output of last op
    bool  showOpLog_ = false;
    int   opLogScroll_ = 0;

    // ---- dialogs ---------------------------------------------------------
    DialogKind dialog_ = DialogKind::None;
    std::wstring dialogTitle_;
    std::wstring dialogFields_[4];
    std::wstring dialogFieldTitles_[4];
    int dialogFieldCount_ = 0;
    int dialogFieldFocus_ = 0;
    int dialogFieldCursor_ = 0;
    std::wstring dialogButtonTitle_;
    int  dialogButtons_ = 0;       // 1 or 2
    int  dialogButtonFocus_ = 0;
    bool dialogConfirm_ = false;
    // For selectable dialogs (e.g. merge branch).
    std::vector<std::wstring> dialogOptions_;
    int dialogOptionIdx_ = 0;
    int dialogOptionScroll_ = 0;
    // Private member used by onMouseClick hit tests.
    struct HitBox
    {
        int row, x, w;
        std::wstring id;   // "nav", "file", "btn", "field", "diff", "commit"
        int index;
    };
    std::vector<HitBox> hitBoxes_;

    // ---- internal helpers ------------------------------------------------
    void refreshStatus();                 // fast status only (used by refresh())
    void loadSectionData();               // loads data for section_ (async for slow)
    void loadFullStatus();               // status + remotes
    void loadFiles();
    void loadDiff();
    void loadBranches();
    void loadHistory();
    void loadGraph();
    void loadRemotes();
    void loadCommitDetail();
    void loadGitHubMeta();                // account + repos
    void startOp(std::unique_ptr<AsyncOp> op);
    void applyOp();                       // dispatch finished op result
    void showNotice(const std::wstring& msg);
    void openDialog(DialogKind kind);
    void closeDialog();
    void applyDialogOK();
    void runGitOp(const std::vector<std::wstring>& args, const std::wstring& label,
                  std::wstring extraPath = L"");
    void runGhOp(const std::wstring& action, const std::wstring& repo = L"",
                 const std::wstring& a = L"", const std::wstring& b = L"",
                 const std::wstring& c = L"", const std::wstring& d = L"");
    void reloadAfterOp();                 // refresh all domain data after success
    void setDiffError(const std::wstring& msg);
    void commitOnly();
    void commitAndPush();
    void stageToggleSelected();
    void discardSelected();
    void dispatchButton(int index);
    bool editField(std::wstring& buf, int& cur, const KeyEvent& key);
    std::wstring currentBranchName() const;

    // drawing
    void drawNav(RenderContext& rc);
    void drawStatusStrip(RenderContext& rc);
    void drawOverview(RenderContext& rc);
    void drawChanges(RenderContext& rc);
    void drawDiff(RenderContext& rc, int row0, int col0, int w, int h,
                  const std::vector<git::GitDiffLine>& diff, int& usedRows);
    void drawBranches(RenderContext& rc);
    void drawHistory(RenderContext& rc);
    void drawGraph(RenderContext& rc);
    void drawRemotes(RenderContext& rc);
    void drawGitHub(RenderContext& rc);
    void drawDialog(RenderContext& rc);
    void drawButtons(RenderContext& rc, int row, const std::vector<std::wstring>& labels, int& usedRows);
    void drawDull(RenderContext& rc, int row0, int col0, int w, int h);

    // keyboard
    void keyOverview(const KeyEvent& key);
    void keyChanges(const KeyEvent& key);
    void keyBranches(const KeyEvent& key);
    void keyHistory(const KeyEvent& key);
    void keyGraph(const KeyEvent& key);
    void keyRemotes(const KeyEvent& key);
    void keyGitHub(const KeyEvent& key);
    void keyDialog(const KeyEvent& key);

    // browser
    void openUrl(const std::wstring& url);
    std::wstring remoteBrowserUrl() const;
};

// Jobs panel.
class JobsPane : public Pane
{
public:
    JobsPane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    void refresh() override;
    void setJobsManager(kshell::JobManager* jm) { jobs_ = jm; }

private:
    kshell::JobManager* jobs_ = nullptr;
};

// History panel.
class HistoryPane : public Pane
{
public:
    HistoryPane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    void refresh() override;
    void setHistory(kshell::History* h) { history_ = h; }

private:
    kshell::History* history_ = nullptr;
    int scrollOffset_ = 0;
    std::wstring filter_;
};

// Environment Variables panel.
class EnvPane : public Pane
{
public:
    EnvPane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    void refresh() override;
    void setEnvironment(kshell::Environment* env) { env_ = env; }

private:
    kshell::Environment* env_ = nullptr;
    int scrollOffset_ = 0;
    std::wstring filter_;
};

// Tracked shell variables panel.
class VariablesPane : public Pane
{
public:
    VariablesPane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    void refresh() override;
    void setTracker(kshell::VariableTracker* vt) { tracker_ = vt; }

private:
    kshell::VariableTracker* tracker_ = nullptr;
    int scrollOffset_ = 0;
    int selectedIdx_ = 0;
    std::wstring filter_;
};

// Command/execution trace panel.
class TracePane : public Pane
{
public:
    TracePane();
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    void refresh() override;
    void setTraceLog(kshell::TraceLog* log) { trace_ = log; }
    void setFilter(int filter); // 0=all, 1=command, 2=execution, 3=variable, 4=directory

private:
    kshell::TraceLog* trace_ = nullptr;
    int scrollOffset_ = 0;
    int filter_ = 0;
};

} // namespace kshell::ui
