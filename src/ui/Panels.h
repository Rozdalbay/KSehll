#pragma once

#include "ui/Ui.h"
#include "ui/Key.h"
#include "system/SystemMonitor.h"

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
    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    void refresh() override;

    void setWorkDir(const std::wstring& dir);

private:
    std::wstring workDir_;
    bool isRepo_ = false;
    std::wstring branch_;
    int ahead_ = 0;
    int behind_ = 0;
    int staged_ = 0;
    int modified_ = 0;
    int untracked_ = 0;
    std::vector<std::wstring> changedFiles_;
    bool needsRefresh_ = true;
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
