#include "ui/Panels.h"

#include "utils/PathUtils.h"
#include "utils/StringUtils.h"
#include "process/ProcessManager.h"
#include "system/SystemMonitor.h"
#include "git/Git.h"
#include "jobs/JobManager.h"
#include "history/History.h"
#include "environment/Environment.h"
#include "core/VariableTracker.h"
#include "core/TraceLog.h"
#include "core/Locale.h"

#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace kshell::ui
{

// =========================================================================
// FilePane
// =========================================================================
FilePane::FilePane() : Pane(L"files")
{
    title_ = L"Files";
    currentDir_ = pathutils::getCurrentDirectory();
    refresh();
}

void FilePane::navigateTo(const std::wstring& dir)
{
    currentDir_ = dir;
    selectedIdx_ = 0;
    scrollOffset_ = 0;
    refresh();
}

void FilePane::refresh()
{
    entries_.clear();
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(currentDir_, ec))
    {
        std::wstring name = e.path().filename().wstring();
        if (!showHidden_ && !name.empty() && name[0] == L'.')
        {
            continue;
        }
        bool isDir = e.is_directory(ec);
        entries_.push_back({name, isDir});
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const FileEntry& a, const FileEntry& b) {
                  if (a.isDir != b.isDir)
                  {
                      return a.isDir;
                  }
                  std::wstring la = a.name;
                  std::wstring lb = b.name;
                  std::transform(la.begin(), la.end(), la.begin(), ::towlower);
                  std::transform(lb.begin(), lb.end(), lb.begin(), ::towlower);
                  return la < lb;
              });
    if (selectedIdx_ >= (int)entries_.size())
    {
        selectedIdx_ = (int)entries_.size() - 1;
        if (selectedIdx_ < 0) selectedIdx_ = 0;
    }
}

void FilePane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Files") + L"  " + currentDir_);

    int listH = rc.bounds.h - 2;
    if (listH <= 0)
    {
        return;
    }

    if (selectedIdx_ >= scrollOffset_ + listH)
    {
        scrollOffset_ = selectedIdx_ - listH + 1;
    }
    if (selectedIdx_ < scrollOffset_)
    {
        scrollOffset_ = selectedIdx_;
    }

    for (int i = 0; i < listH && (i + scrollOffset_) < (int)entries_.size(); ++i)
    {
        int idx = i + scrollOffset_;
        const auto& entry = entries_[idx];
        bool selected = (idx == selectedIdx_);
        render::Role fgRole = entry.isDir ? render::Role::Accent : render::Role::Foreground;
        std::wstring display = (entry.isDir ? L"[D] " : L"    ") + entry.name;
        if ((int)display.size() > rc.bounds.w - 2)
        {
            display = display.substr(0, rc.bounds.w - 5) + L"...";
        }
        draw::text(rc, i + 1, 1, display, selected ? render::Role::AccentText : fgRole,
                   selected ? render::Role::Selection : render::Role::Background,
                   selected);
    }

    // Footer with clickable actions.
    std::wstring footer = L" [H]hidden   [Enter]open   [F5]refresh";
    draw::text(rc, rc.bounds.h - 1, 1, footer, render::Role::Muted);
}

void FilePane::openSelected()
{
    if (selectedIdx_ < 0 || selectedIdx_ >= (int)entries_.size())
    {
        return;
    }
    const auto& entry = entries_[selectedIdx_];
    fs::path target = fs::path(currentDir_) / entry.name;
    if (entry.isDir)
    {
        navigateTo(target.wstring());
    }
    else
    {
        // Open a file with the system default application.
        ShellExecuteW(nullptr, L"open", target.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void FilePane::toggleHidden()
{
    showHidden_ = !showHidden_;
    refresh();
}

bool FilePane::onKey(const KeyEvent& key)
{
    if (key.key == Key::Up || (key.ctrl && key.ch == L'p'))
    {
        if (selectedIdx_ > 0)
        {
            --selectedIdx_;
        }
        return true;
    }
    if (key.key == Key::Down || (key.ctrl && key.ch == L'n'))
    {
        if (selectedIdx_ < (int)entries_.size() - 1)
        {
            ++selectedIdx_;
        }
        return true;
    }
    if (key.key == Key::Home)
    {
        selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::End)
    {
        selectedIdx_ = (int)entries_.size() - 1;
        if (selectedIdx_ < 0) selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::PageUp)
    {
        selectedIdx_ -= 10;
        if (selectedIdx_ < 0) selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::PageDown)
    {
        selectedIdx_ += 10;
        if (selectedIdx_ >= (int)entries_.size()) selectedIdx_ = (int)entries_.size() - 1;
        if (selectedIdx_ < 0) selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::Backspace)
    {
        fs::path parent = fs::path(currentDir_).parent_path();
        if (parent != currentDir_)
        {
            currentDir_ = parent.wstring();
            selectedIdx_ = 0;
            scrollOffset_ = 0;
            refresh();
        }
        return true;
    }
    if (key.key == Key::Enter || key.key == Key::Space)
    {
        openSelected();
        return true;
    }
    if (key.ch == L'h' || key.ch == L'H')
    {
        toggleHidden();
        return true;
    }
    if (key.key == Key::F5)
    {
        refresh();
        return true;
    }
    if (key.key == Key::F2)
    {
        // Toggle a filter input cursor hint (unused placeholder).
        return true;
    }
    return false;
}

void FilePane::onMouseWheel(int delta)
{
    // Step by 1 for smooth scrolling; clamped to valid selection range.
    if (delta > 0)
    {
        if (selectedIdx_ > 0)
        {
            --selectedIdx_;
        }
    }
    else if (delta < 0)
    {
        if (selectedIdx_ < (int)entries_.size() - 1)
        {
            ++selectedIdx_;
        }
    }
}

bool FilePane::onMouseClick(int rowInPane, int colInPane, bool doubleClick)
{
    (void)colInPane;
    // The first drawn row (index 0) is at row 1 in pane coords (header at row 0).
    int listRow = rowInPane - 1;
    if (listRow >= 0 && listRow + scrollOffset_ < (int)entries_.size())
    {
        selectedIdx_ = listRow + scrollOffset_;
        if (doubleClick)
        {
            openSelected();
        }
        return true;
    }
    return false;
}

// =========================================================================
// ProcessPane
// =========================================================================
ProcessPane::ProcessPane() : Pane(L"processes")
{
    title_ = L"Processes";
}

void ProcessPane::refresh()
{
    ULONGLONG now = ::GetTickCount64();
    if (now - lastRefreshTime_ < 500)
    {
        return;
    }
    lastRefreshTime_ = now;

    process::ProcessManager pm;
    pm.primeCpuSample();
    auto snap = pm.snapshot();
    procs_.clear();
    for (const auto& p : snap)
    {
        if (!filter_.empty() && p.name.find(filter_) == std::wstring::npos &&
            std::to_wstring(p.pid).find(filter_) == std::wstring::npos)
        {
            continue;
        }
        procs_.push_back({p.pid, p.parentPid, p.name, p.cpuPercent, p.workingSetBytes, 0});
    }
    rebuildTree();
    needsRefresh_ = false;
}

void ProcessPane::rebuildTree()
{
    if (!treeMode_)
    {
        std::sort(procs_.begin(), procs_.end(),
                  [](const ProcEntry& a, const ProcEntry& b) { return a.ram > b.ram; });
        return;
    }

    // Preserve parentPid and compute tree depth/order.
    std::vector<ProcEntry> ordered;
    std::unordered_map<uint32_t, std::vector<size_t>> children;
    std::unordered_map<uint32_t, size_t> indexByPid;
    for (size_t i = 0; i < procs_.size(); ++i)
    {
        indexByPid[procs_[i].pid] = i;
    }
    for (const auto& e : procs_)
    {
        children[e.parentPid].push_back(indexByPid[e.pid]);
    }

    std::vector<size_t> roots = children[0];
    std::unordered_set<uint32_t> hasParent;
    for (const auto& e : procs_)
    {
        hasParent.insert(e.parentPid);
    }
    // Roots: processes whose parent is not visible in the list (or is 0).
    std::vector<size_t> realRoots;
    for (size_t i = 0; i < procs_.size(); ++i)
    {
        if (procs_[i].parentPid == 0 || indexByPid.find(procs_[i].parentPid) == indexByPid.end())
        {
            realRoots.push_back(i);
        }
    }
    std::sort(realRoots.begin(), realRoots.end(),
              [&](size_t a, size_t b) { return procs_[a].name < procs_[b].name; });

    std::vector<ProcEntry> result;
    std::unordered_set<uint32_t> visited;
    // Iterative stack-based DFS to avoid stack overflow on deep process trees.
    struct Frame { size_t idx; int depth; };
    std::vector<Frame> stack;
    for (auto it = realRoots.rbegin(); it != realRoots.rend(); ++it)
    {
        stack.push_back({*it, 0});
    }
    while (!stack.empty())
    {
        Frame f = stack.back();
        stack.pop_back();
        if (visited.count(procs_[f.idx].pid))
        {
            continue;
        }
        visited.insert(procs_[f.idx].pid);
        ProcEntry e = procs_[f.idx];
        e.depth = f.depth;
        result.push_back(e);
        auto cit = children.find(e.pid);
        if (cit != children.end())
        {
            std::vector<size_t>& ch = cit->second;
            std::sort(ch.begin(), ch.end(),
                      [&](size_t a, size_t b) { return procs_[a].name < procs_[b].name; });
            for (auto it = ch.rbegin(); it != ch.rend(); ++it)
            {
                if (!visited.count(procs_[*it].pid))
                {
                    stack.push_back({*it, f.depth + 1});
                }
            }
        }
    }
    procs_ = std::move(result);
}

void ProcessPane::draw(RenderContext& rc)
{
    if (needsRefresh_)
    {
        refresh();
    }

    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Processes") + L"  (" + std::to_wstring(procs_.size()) +
                     L")  " + tr(L"[F]force kill  [Del]kill  [T]tree"));
    int listH = rc.bounds.h - 2;
    if (listH <= 0)
    {
        return;
    }

    if (selectedIdx_ >= scrollOffset_ + listH)
    {
        scrollOffset_ = selectedIdx_ - listH + 1;
    }
    if (selectedIdx_ < scrollOffset_)
    {
        scrollOffset_ = selectedIdx_;
    }

    // Column headers.
    std::vector<int> widths = {8, 34, 7, 10};
    std::vector<std::wstring> headerRow = {L"PID", L"Name", L"CPU%", L"RAM"};
    draw::tableRow(rc, 0, headerRow, widths, render::Role::Muted, render::Role::Background, false);

    for (int i = 0; i < listH && (i + scrollOffset_) < (int)procs_.size(); ++i)
    {
        int idx = i + scrollOffset_;
        const auto& p = procs_[idx];
        bool selected = (idx == selectedIdx_);

        wchar_t cpuStr[16];
        swprintf(cpuStr, 16, L"%.1f%%", p.cpu);
        wchar_t ramStr[16];
        if (p.ram > 1024 * 1024)
        {
            swprintf(ramStr, 16, L"%.1fM", (double)p.ram / (1024.0 * 1024.0));
        }
        else
        {
            swprintf(ramStr, 16, L"%.0fK", (double)p.ram / 1024.0);
        }

        std::wstring name = p.name;
        if (treeMode_)
        {
            name = std::wstring(2 * p.depth, L' ') + (p.depth > 0 ? L"\u2514" : L"") + name;
        }

        std::vector<std::wstring> cells = {
            std::to_wstring(p.pid),
            name,
            cpuStr,
            ramStr,
        };
        draw::tableRow(rc, i + 1, cells, widths, render::Role::Foreground,
                       render::Role::Background, selected);
    }
}

bool ProcessPane::onKey(const KeyEvent& key)
{
    if (key.key == Key::Up)
    {
        if (selectedIdx_ > 0)
        {
            --selectedIdx_;
        }
        return true;
    }
    if (key.key == Key::Down)
    {
        if (selectedIdx_ < (int)procs_.size() - 1)
        {
            ++selectedIdx_;
        }
        return true;
    }
    if (key.key == Key::Home)
    {
        selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::End)
    {
        selectedIdx_ = (int)procs_.size() - 1;
        if (selectedIdx_ < 0) selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::PageUp)
    {
        selectedIdx_ -= 10;
        if (selectedIdx_ < 0) selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::PageDown)
    {
        selectedIdx_ += 10;
        if (selectedIdx_ >= (int)procs_.size()) selectedIdx_ = (int)procs_.size() - 1;
        if (selectedIdx_ < 0) selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::F5)
    {
        refresh();
        return true;
    }
    if (key.key == Key::F || key.key == Key::Delete)
    {
        if (selectedIdx_ >= 0 && selectedIdx_ < (int)procs_.size())
        {
            process::ProcessManager pm;
            process::TerminateAction act = (key.key == Key::F)
                                               ? process::TerminateAction::Force
                                               : process::TerminateAction::Request;
            pm.terminate(procs_[selectedIdx_].pid, act);
            refresh();
        }
        return true;
    }
    if (key.ch == L't' || key.ch == L'T')
    {
        treeMode_ = !treeMode_;
        rebuildTree();
        return true;
    }
    return false;
}

void ProcessPane::onMouseWheel(int delta)
{
    // Step by 1 for smooth scrolling.
    if (delta > 0)
    {
        if (selectedIdx_ > 0)
        {
            --selectedIdx_;
        }
    }
    else if (delta < 0)
    {
        if (selectedIdx_ < (int)procs_.size() - 1)
        {
            ++selectedIdx_;
        }
    }
}

bool ProcessPane::onMouseClick(int rowInPane, int colInPane, bool doubleClick)
{
    (void)colInPane;
    (void)doubleClick;
    // Header at row 0, list starts at row 1.
    int listRow = rowInPane - 1;
    if (listRow >= 0 && listRow + scrollOffset_ < (int)procs_.size())
    {
        selectedIdx_ = listRow + scrollOffset_;
        return true;
    }
    return false;
}

// =========================================================================
// SystemPane
// =========================================================================
SystemPane::SystemPane()
    : Pane(L"system")
{
    title_ = L"System";
}

void SystemPane::refresh()
{
    // monitor_ is persistent across refresh() calls so the first CPU sample
    // primes instead of always reading 0%.
    auto s = monitor_.refresh();
    cpu_ = s.cpuPercent;
    ram_ = s.ramPercent;
    ramUsed_ = s.ramUsedBytes;
    ramTotal_ = s.ramTotalBytes;
    disk_ = s.diskPercent;
    diskFree_ = s.diskFreeBytes;
    diskTotal_ = s.diskTotalBytes;
    procCount_ = s.processCount;
    uptime_ = s.uptimeSeconds;
    cpuHistory_ = s.cpuHistory;
    ramHistory_ = s.ramHistory;
    needsRefresh_ = false;
}

void SystemPane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"System Monitor"));

    int row = 2;
    auto printKV = [&](const std::wstring& k, const std::wstring& v) {
        draw::text(rc, row, 2, k, render::Role::Muted);
        draw::text(rc, row, 20, v, render::Role::Foreground);
        ++row;
    };

    auto formatBytes = [](uint64_t b) -> std::wstring {
        const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
        double v = (double)b;
        int u = 0;
        while (v >= 1024.0 && u < 4)
        {
            v /= 1024.0;
            ++u;
        }
        wchar_t buf[64];
        swprintf(buf, 64, L"%.1f %ls", v, units[u]);
        return buf;
    };

    // Helper to draw a horizontal real-time sparkline from a history vector.
    // Values are percentages in [0,100]; each column maps to a block char.
    auto sparkline = [&](int y, const std::vector<double>& hist, const std::wstring& label,
                         render::Role role) {
        // Legend.
        draw::text(rc, y, 2, label, render::Role::Muted);
        int plotX = 20;
        int plotW = rc.bounds.w - plotX - 2;
        if (plotW < 10)
        {
            plotW = 10;
        }
        if (plotW > 0 && (int)hist.size() > 0)
        {
            int n = (int)hist.size();
            for (int i = 0; i < plotW; ++i)
            {
                int idx = i + (n - plotW);
                if (idx < 0)
                {
                    idx = 0;
                }
                double v = hist[idx] / 100.0;
                if (v < 0.0) v = 0.0;
                if (v > 1.0) v = 1.0;
                // Use ascending 1/8 block levels.
                int level = (int)(v * 8.0);
                if (level < 0) level = 0;
                if (level > 8) level = 8;
                if (level == 0)
                {
                    rc.screen.put(rc.bounds.y + y, rc.bounds.x + plotX + i, L'\u2591', t.color(render::Role::Muted), t.color(render::Role::Background));
                }
                else if (level <= 8)
                {
                    wchar_t chs[9] = {L'\u2581', L'\u2581', L'\u2582', L'\u2583',
                                      L'\u2584', L'\u2585', L'\u2586', L'\u2587', L'\u2588'};
                    rc.screen.put(rc.bounds.y + y, rc.bounds.x + plotX + i, chs[level - 1], t.color(role), t.color(render::Role::Background));
                }
            }
        }
    };

    printKV(L"CPU", std::to_wstring((int)cpu_) + L"%");
    draw::bar(rc, row, 2, 20, cpu_ / 100.0, render::Role::Accent, render::Role::Muted);
    ++row;
    sparkline(row, cpuHistory_, L"  trend:", render::Role::Accent);
    row += 2;

    printKV(L"RAM", formatBytes(ramUsed_) + L" / " + formatBytes(ramTotal_) +
           L" (" + std::to_wstring((int)ram_) + L"%)");
    draw::bar(rc, row, 2, 20, ram_ / 100.0, render::Role::Success, render::Role::Muted);
    ++row;
    sparkline(row, ramHistory_, L"  trend:", render::Role::Success);
    row += 2;

    printKV(L"Disk", formatBytes(diskFree_) + L" free / " + formatBytes(diskTotal_) +
           L" (" + std::to_wstring((int)disk_) + L"% used)");
    draw::bar(rc, row, 2, 20, disk_ / 100.0, render::Role::Warning, render::Role::Muted);
    row += 2;

    printKV(L"Processes", std::to_wstring(procCount_));

    wchar_t uptimeBuf[64];
    uint64_t days = uptime_ / 86400;
    uint64_t hours = (uptime_ % 86400) / 3600;
    uint64_t mins = (uptime_ % 3600) / 60;
    swprintf(uptimeBuf, 64, L"%llud %lluh %llum", days, hours, mins);
    printKV(L"Uptime", uptimeBuf);

    draw::text(rc, rc.bounds.h - 1, 2, tr(L"[F5]refresh  CPU/RAM history updates live"), render::Role::Muted);
}

bool SystemPane::onKey(const KeyEvent& key)
{
    if (key.key == Key::F5)
    {
        refresh();
        return true;
    }
    return false;
}

void SystemPane::onMouseWheel(int delta) { (void)delta; }

// =========================================================================
// GitPane
// =========================================================================
GitPane::GitPane() : Pane(L"git")
{
    title_ = L"Git";
    workDir_ = pathutils::getCurrentDirectory();
    refresh();
}

void GitPane::setWorkDir(const std::wstring& dir)
{
    workDir_ = dir;
    needsRefresh_ = true;
}

void GitPane::refresh()
{
    git::Git g;
    auto status = g.status(workDir_);
    isRepo_ = status.isRepo;
    branch_ = status.branch;
    ahead_ = status.ahead;
    behind_ = status.behind;
    staged_ = status.staged;
    modified_ = status.modified;
    untracked_ = status.untracked;
    changedFiles_ = status.changedFiles;
    needsRefresh_ = false;
}

void GitPane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Git"));

    if (!isRepo_)
    {
        draw::text(rc, 2, 2, L"Not a git repository", render::Role::Muted);
        return;
    }

    int row = 2;
    auto printKV = [&](const std::wstring& k, const std::wstring& v, render::Role fg = render::Role::Foreground) {
        draw::text(rc, row, 2, k, render::Role::Muted);
        draw::text(rc, row, 18, v, fg);
        ++row;
    };

    printKV(L"Branch", branch_, render::Role::Accent);
    if (ahead_ > 0)
    {
        printKV(L"Ahead", std::to_wstring(ahead_), render::Role::Success);
    }
    if (behind_ > 0)
    {
        printKV(L"Behind", std::to_wstring(behind_), render::Role::Warning);
    }
    if (staged_ > 0)
    {
        printKV(L"Staged", std::to_wstring(staged_), render::Role::Success);
    }
    if (modified_ > 0)
    {
        printKV(L"Modified", std::to_wstring(modified_), render::Role::Warning);
    }
    if (untracked_ > 0)
    {
        printKV(L"Untracked", std::to_wstring(untracked_), render::Role::Muted);
    }
    ++row;

    int maxFiles = rc.bounds.h - row - 1;
    if (maxFiles > 0 && !changedFiles_.empty())
    {
        draw::text(rc, row, 2, L"Changed Files:", render::Role::Muted);
        ++row;
        for (int i = 0; i < maxFiles && i < (int)changedFiles_.size(); ++i)
        {
            std::wstring name = changedFiles_[i];
            if ((int)name.size() > rc.bounds.w - 4)
            {
                name = name.substr(0, rc.bounds.w - 7) + L"...";
            }
            draw::text(rc, row + i, 4, name, render::Role::Foreground);
        }
    }
}

bool GitPane::onKey(const KeyEvent& key)
{
    if (key.key == Key::F5)
    {
        refresh();
        return true;
    }
    return false;
}

void GitPane::onMouseWheel(int delta) { (void)delta; }

// =========================================================================
// JobsPane
// =========================================================================
JobsPane::JobsPane() : Pane(L"jobs")
{
    title_ = L"Jobs";
}

void JobsPane::refresh()
{
    if (jobs_)
    {
        jobs_->updateStates();
    }
}

void JobsPane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Jobs"));

    if (!jobs_)
    {
        draw::text(rc, 2, 2, L"No job manager set", render::Role::Muted);
        return;
    }

    const auto& jobs = jobs_->getAll();
    if (jobs.empty())
    {
        draw::text(rc, 2, 2, L"No background jobs", render::Role::Muted);
        return;
    }

    int row = 2;
    std::vector<int> widths = {5, 35, 10};
    std::vector<std::wstring> headerRow = {L"ID", L"Command", L"Status"};
    draw::tableRow(rc, 0, headerRow, widths, render::Role::Muted, render::Role::Background, false);

    int listH = rc.bounds.h - 3;
    for (int i = 0; i < listH && i < (int)jobs.size(); ++i)
    {
        const auto& j = jobs[i];
        std::wstring statusStr;
        render::Role statusFg = render::Role::Foreground;
        switch (j.state)
        {
        case JobState::Running:
            statusStr = L"Running";
            statusFg = render::Role::Success;
            break;
        case JobState::Finished:
            statusStr = L"Done";
            break;
        case JobState::Failed:
            statusStr = L"Failed";
            statusFg = render::Role::Error;
            break;
        case JobState::Terminated:
            statusStr = L"Killed";
            statusFg = render::Role::Warning;
            break;
        }

        std::vector<std::wstring> cells = {
            std::to_wstring(j.id),
            j.command,
            statusStr,
        };
        draw::tableRow(rc, i + 1, cells, widths, statusFg, render::Role::Background, false);
    }
}

bool JobsPane::onKey(const KeyEvent& key)
{
    return false;
}

void JobsPane::onMouseWheel(int delta) { (void)delta; }

// =========================================================================
// HistoryPane
// =========================================================================
HistoryPane::HistoryPane() : Pane(L"history")
{
    title_ = L"History";
}

void HistoryPane::refresh() {}

void HistoryPane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Command History"));

    if (!history_)
    {
        draw::text(rc, 2, 2, L"No history set", render::Role::Muted);
        return;
    }

    const auto& entries = history_->entries();
    int listH = rc.bounds.h - 2;
    if (listH <= 0)
    {
        return;
    }

    if (scrollOffset_ >= (int)entries.size() - listH)
    {
        scrollOffset_ = (int)entries.size() - listH;
    }
    if (scrollOffset_ < 0)
    {
        scrollOffset_ = 0;
    }

    for (int i = 0; i < listH && (i + scrollOffset_) < (int)entries.size(); ++i)
    {
        int idx = i + scrollOffset_;
        int num = (int)entries.size() - idx;
        std::wstring display = std::to_wstring(num) + L"  " + entries[idx];
        if ((int)display.size() > rc.bounds.w - 2)
        {
            display = display.substr(0, rc.bounds.w - 5) + L"...";
        }
        draw::text(rc, i + 1, 1, display, render::Role::Foreground);
    }
}

bool HistoryPane::onKey(const KeyEvent& key)
{
    if (key.key == Key::Up)
    {
        if (scrollOffset_ > 0)
        {
            --scrollOffset_;
        }
        return true;
    }
    if (key.key == Key::Down)
    {
        ++scrollOffset_;
        return true;
    }
    if (key.key == Key::PageUp)
    {
        scrollOffset_ -= 10;
        if (scrollOffset_ < 0) scrollOffset_ = 0;
        return true;
    }
    if (key.key == Key::PageDown)
    {
        scrollOffset_ += 10;
        return true;
    }
    return false;
}

void HistoryPane::onMouseWheel(int delta)
{
    if (delta > 0)
    {
        scrollOffset_ -= 3;
        if (scrollOffset_ < 0) scrollOffset_ = 0;
    }
    else if (delta < 0)
    {
        scrollOffset_ += 3;
    }
}

// =========================================================================
// EnvPane
// =========================================================================
EnvPane::EnvPane() : Pane(L"environment")
{
    title_ = L"Environment";
}

void EnvPane::refresh()
{
    // Env vars are read live in draw(); nothing to cache here.
}

void EnvPane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Environment Variables  [F5]refresh  [/]filter"));

    if (!env_)
    {
        draw::text(rc, 2, 2, tr(L"No environment set"), render::Role::Muted);
        return;
    }

    auto all = env_->getAll();
    int listH = rc.bounds.h - 2;
    if (listH <= 0)
    {
        return;
    }

    // Filter and sort.
    std::vector<std::pair<std::wstring, std::wstring>> filtered;
    for (const auto& [k, v] : all)
    {
        if (!filter_.empty() && k.find(filter_) == std::wstring::npos)
        {
            continue;
        }
        filtered.emplace_back(k, v);
    }
    std::sort(filtered.begin(), filtered.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    if (scrollOffset_ >= (int)filtered.size() - listH)
    {
        scrollOffset_ = (int)filtered.size() - listH;
    }
    if (scrollOffset_ < 0)
    {
        scrollOffset_ = 0;
    }

    for (int i = 0; i < listH && (i + scrollOffset_) < (int)filtered.size(); ++i)
    {
        int idx = i + scrollOffset_;
        const auto& [k, v] = filtered[idx];
        std::wstring display = k + L"=" + v;
        if ((int)display.size() > rc.bounds.w - 2)
        {
            display = display.substr(0, rc.bounds.w - 5) + L"...";
        }
        // Highlight user-set vars distinctly from inherited system vars.
        render::Role fg = render::Role::Foreground;
        if (env_->isUserSet(k))
        {
            fg = render::Role::Accent; // user-set
        }
        else if (k == L"PATH" || k == L"HOME" || k == L"USERPROFILE")
        {
            fg = render::Role::Highlight;
        }
        draw::text(rc, i + 1, 1, display, fg);
    }

    std::wstring footer = tr(L"Total: ") + std::to_wstring(filtered.size());
    if (!filter_.empty())
    {
        footer += L"   filter: " + filter_;
    }
    draw::text(rc, rc.bounds.h - 1, 1, footer, render::Role::Muted);
}

bool EnvPane::onKey(const KeyEvent& key)
{
    if (key.key == Key::Up)
    {
        if (scrollOffset_ > 0)
        {
            --scrollOffset_;
        }
        return true;
    }
    if (key.key == Key::Down)
    {
        ++scrollOffset_;
        return true;
    }
    if (key.key == Key::PageUp)
    {
        scrollOffset_ -= 10;
        if (scrollOffset_ < 0) scrollOffset_ = 0;
        return true;
    }
    if (key.key == Key::PageDown)
    {
        scrollOffset_ += 10;
        return true;
    }
    if (key.key == Key::Backspace)
    {
        if (!filter_.empty())
        {
            filter_.pop_back();
            scrollOffset_ = 0;
        }
        return true;
    }
    if (key.key == Key::Slash)
    {
        filter_.clear();
        return true;
    }
    if (key.isPrint())
    {
        filter_.push_back(key.ch);
        scrollOffset_ = 0;
        return true;
    }
    return false;
}

void EnvPane::onMouseWheel(int delta)
{
    if (delta > 0)
    {
        scrollOffset_ -= 3;
        if (scrollOffset_ < 0) scrollOffset_ = 0;
    }
    else if (delta < 0)
    {
        scrollOffset_ += 3;
    }
}

// =========================================================================
// VariablesPane
// =========================================================================
VariablesPane::VariablesPane() : Pane(L"variables")
{
    title_ = L"Variables";
}

void VariablesPane::refresh() {}

void VariablesPane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Tracked Variables  [Enter]history  [F5]refresh"));

    if (!tracker_)
    {
        draw::text(rc, 2, 2, tr(L"No variable tracker set"), render::Role::Muted);
        return;
    }

    auto vars = tracker_->snapshot();
    if (vars.empty())
    {
        draw::text(rc, 2, 2, tr(L"No tracked variables. Use 'set NAME=value'."), render::Role::Muted);
        return;
    }

    int listH = rc.bounds.h - 2;
    if (listH <= 0)
    {
        return;
    }

    std::vector<const TrackedVariable*> filtered;
    for (const auto& v : vars)
    {
        if (!filter_.empty() && v.name.find(filter_) == std::wstring::npos)
        {
            continue;
        }
        filtered.push_back(&v);
    }
    std::sort(filtered.begin(), filtered.end(),
              [](const auto* a, const auto* b) { return a->name < b->name; });

    if (selectedIdx_ >= (int)filtered.size())
    {
        selectedIdx_ = (int)filtered.size() - 1;
    }
    if (selectedIdx_ < 0) selectedIdx_ = 0;
    if (selectedIdx_ >= scrollOffset_ + listH)
    {
        scrollOffset_ = selectedIdx_ - listH + 1;
    }
    if (selectedIdx_ < scrollOffset_)
    {
        scrollOffset_ = selectedIdx_;
    }

    draw::tableRow(rc, 0, {L"Name", L"Value", L"History"}, {14, (rc.bounds.w - 26), 8},
                   render::Role::Muted, render::Role::Background, false);

    for (int i = 0; i < listH && (i + scrollOffset_) < (int)filtered.size(); ++i)
    {
        int idx = i + scrollOffset_;
        const auto* v = filtered[idx];
        bool selected = (idx == selectedIdx_);
        std::wstring value = v->currentValue;
        if ((int)value.size() > rc.bounds.w - 24)
        {
            value = value.substr(0, rc.bounds.w - 27) + L"...";
        }
        std::wstring historyCount = std::to_wstring(v->history.size());
        draw::tableRow(rc, i + 1, {v->name, value, historyCount},
                       {14, (rc.bounds.w - 26), 8},
                       selected ? render::Role::Accent : render::Role::Foreground,
                       render::Role::Background, selected);
    }

    draw::text(rc, rc.bounds.h - 1, 1,
               tr(L"Total: ") + std::to_wstring(filtered.size()), render::Role::Muted);
}

bool VariablesPane::onKey(const KeyEvent& key)
{
    if (key.key == Key::Up)
    {
        if (selectedIdx_ > 0) --selectedIdx_;
        return true;
    }
    if (key.key == Key::Down)
    {
        ++selectedIdx_;
        return true;
    }
    if (key.key == Key::PageUp)
    {
        selectedIdx_ -= 10;
        if (selectedIdx_ < 0) selectedIdx_ = 0;
        return true;
    }
    if (key.key == Key::PageDown)
    {
        selectedIdx_ += 10;
        return true;
    }
    if (key.key == Key::Enter && tracker_)
    {
        auto vars = tracker_->snapshot();
        if (selectedIdx_ >= 0 && selectedIdx_ < (int)vars.size())
        {
            const auto& v = vars[selectedIdx_];
            std::wstring msg = L"History for " + v.name + L":";
            for (const auto& h : v.history)
            {
                msg += L"\n  [" + h.timestamp + L"] " + h.value;
            }
            // Show via a simple overlay is not available; print a short inline summary.
            // No-op for now (kept simple).
        }
        return true;
    }
    if (key.key == Key::Backspace)
    {
        if (!filter_.empty())
        {
            filter_.pop_back();
            scrollOffset_ = 0;
            selectedIdx_ = 0;
        }
        return true;
    }
    if (key.isPrint())
    {
        filter_.push_back(key.ch);
        scrollOffset_ = 0;
        selectedIdx_ = 0;
        return true;
    }
    return false;
}

void VariablesPane::onMouseWheel(int delta)
{
    if (delta > 0)
    {
        if (selectedIdx_ > 0) --selectedIdx_;
    }
    else if (delta < 0)
    {
        ++selectedIdx_;
    }
}

// =========================================================================
// TracePane
// =========================================================================
TracePane::TracePane() : Pane(L"trace")
{
    title_ = L"Trace";
}

void TracePane::setFilter(int f)
{
    filter_ = f;
    scrollOffset_ = 0;
}

void TracePane::refresh() {}

void TracePane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Trace  [1]all [2]cmd [3]exec [4]var [5]dir  [F5]refresh"));

    if (!trace_)
    {
        draw::text(rc, 2, 2, tr(L"No trace log set"), render::Role::Muted);
        return;
    }

    std::vector<TraceEvent> all = trace_->events();
    std::vector<const TraceEvent*> filtered;
    for (const auto& e : all)
    {
        if (filter_ == 0) { filtered.push_back(&e); continue; }
        int k = 0;
        switch (e.kind)
        {
        case TraceKind::CommandTrace: k = 1; break;
        case TraceKind::Execution:     k = 2; break;
        case TraceKind::Variable:      k = 3; break;
        case TraceKind::Directory:     k = 4; break;
        }
        if (k == filter_) filtered.push_back(&e);
    }

    int listH = rc.bounds.h - 1;
    if (listH <= 0)
    {
        return;
    }

    if (scrollOffset_ >= (int)filtered.size() - listH)
    {
        scrollOffset_ = (int)filtered.size() - listH;
    }
    if (scrollOffset_ < 0)
    {
        scrollOffset_ = 0;
    }

    for (int i = 0; i < listH && (i + scrollOffset_) < (int)filtered.size(); ++i)
    {
        int idx = i + scrollOffset_;
        const auto* e = filtered[idx];
        render::Role fg = render::Role::Foreground;
        switch (e->kind)
        {
        case TraceKind::CommandTrace: fg = render::Role::Muted; break;
        case TraceKind::Execution:    fg = (e->exitCode == 0) ? render::Role::Success
                                                              : render::Role::Error; break;
        case TraceKind::Variable:     fg = render::Role::Accent; break;
        case TraceKind::Directory:    fg = render::Role::Warning; break;
        }
        std::wstring line = L"[" + e->timestamp + L"] " + e->message;
        if ((int)line.size() > rc.bounds.w - 2)
        {
            line = line.substr(0, rc.bounds.w - 2);
        }
        draw::text(rc, i + 1, 1, line, fg);
    }
}

bool TracePane::onKey(const KeyEvent& key)
{
    if (key.key == Key::Up)
    {
        if (scrollOffset_ > 0) --scrollOffset_;
        return true;
    }
    if (key.key == Key::Down)
    {
        ++scrollOffset_;
        return true;
    }
    if (key.key == Key::PageUp)
    {
        scrollOffset_ -= 10;
        if (scrollOffset_ < 0) scrollOffset_ = 0;
        return true;
    }
    if (key.key == Key::PageDown)
    {
        scrollOffset_ += 10;
        return true;
    }
    if (key.key == Key::F1) { filter_ = 0; scrollOffset_ = 0; return true; }
    if (key.key == Key::F2) { filter_ = 1; scrollOffset_ = 0; return true; }
    if (key.key == Key::F3) { filter_ = 2; scrollOffset_ = 0; return true; }
    if (key.key == Key::F4) { filter_ = 3; scrollOffset_ = 0; return true; }
    if (key.key == Key::F5) { filter_ = 4; scrollOffset_ = 0; return true; }
    return false;
}

void TracePane::onMouseWheel(int delta)
{
    if (delta > 0)
    {
        scrollOffset_ -= 3;
        if (scrollOffset_ < 0) scrollOffset_ = 0;
    }
    else if (delta < 0)
    {
        scrollOffset_ += 3;
    }
}

} // namespace kshell::ui
