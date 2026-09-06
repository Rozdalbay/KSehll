#include "ui/Panels.h"

#include "utils/PathUtils.h"
#include "utils/StringUtils.h"
#include "process/ProcessManager.h"
#include "system/SystemMonitor.h"
#include "git/Git.h"
#include "git/GitHub.h"
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
    // (theme not needed here)
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

    // (theme not needed here)
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
// GitPane - a full Git/GitHub workspace.
// =========================================================================
namespace
{

constexpr int kNavW = 26;
constexpr int kStatusRow = 1;
constexpr int kNavRow = 2;
constexpr int kContentRow = 3;

std::wstring clipRight(const std::wstring& s, size_t max)
{
    if (s.size() <= max)
    {
        return s;
    }
    if (max <= 1)
    {
        return L"\u2026";
    }
    return s.substr(0, max - 1) + L"\u2026";
}

std::wstring padR(const std::wstring& s, int w)
{
    std::wstring r = clipRight(s, (size_t)w);
    if ((int)r.size() < w)
    {
        r.append((size_t)(w - (int)r.size()), L' ');
    }
    return r;
}

std::wstring padL(const std::wstring& s, int w)
{
    std::wstring r = s;
    if ((int)r.size() > w)
    {
        r = r.substr(r.size() - (size_t)w);
    }
    if ((int)r.size() < w)
    {
        r.insert(0, (size_t)(w - (int)r.size()), L' ');
    }
    return r;
}

std::wstring statusMark(wchar_t a, wchar_t b)
{
    std::wstring s;
    s += (a == L'?' || a == L' ') ? L'_' : a;
    s += (b == L'?' || b == L' ') ? L'_' : b;
    return s;
}

render::Role fileRole(const git::GitFileEntry& e)
{
    if (e.untracked)
    {
        return render::Role::Muted;
    }
    if (e.indexStatus == L'U' || e.workTreeStatus == L'U' || e.indexStatus == L'u' || e.workTreeStatus == L'u')
    {
        return render::Role::Warning;
    }
    if (e.workTreeStatus == L'D' || e.indexStatus == L'D')
    {
        return render::Role::Error;
    }
    if (e.staged)
    {
        return render::Role::Success;
    }
    return render::Role::Warning;
}

// ssh://git@host/owner/repo.git and git@host:owner/repo.git -> https URL.
std::wstring browserUrl(const std::wstring& remote)
{
    std::wstring r = stringutils::trim(remote);
    if (r.empty())
    {
        return L"";
    }
    if (stringutils::startsWith(r, L"git@"))
    {
        size_t i = r.find(L':');
        if (i != std::wstring::npos)
        {
            std::wstring host = r.substr(4, i - 4);
            r = L"https://" + host + L"/" + r.substr(i + 1);
        }
    }
    else if (stringutils::startsWith(r, L"ssh://"))
    {
        size_t p = r.find(L'/');
        if (p != std::wstring::npos)
        {
            std::wstring rest = r.substr(p + 1);
            size_t q = rest.find(L'/');
            if (q != std::wstring::npos)
            {
                r = L"https://" + rest.substr(0, q) + L"/" + rest.substr(q + 1);
            }
        }
    }
    if (stringutils::endsWith(r, L".git"))
    {
        r = r.substr(0, r.size() - 4);
    }
    return r;
}

void setClipboard(const std::wstring& text)
{
    if (!::OpenClipboard(nullptr))
    {
        return;
    }
    ::EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h)
    {
        void* p = ::GlobalLock(h);
        if (p)
        {
            memcpy(p, text.c_str(), bytes);
            ::GlobalUnlock(h);
            ::SetClipboardData(CF_UNICODETEXT, h);
        }
        else
        {
            ::GlobalFree(h);
        }
    }
    ::CloseClipboard();
}

void ensureVisible(int& scroll, int sel, int page)
{
    if (sel < scroll)
    {
        scroll = sel;
    }
    if (page > 0 && sel >= scroll + page)
    {
        scroll = sel - page + 1;
    }
    if (scroll < 0)
    {
        scroll = 0;
    }
}

} // namespace

GitPane::GitPane() : Pane(L"git")
{
    title_ = L"Git";
    workDir_ = pathutils::getCurrentDirectory();
    refresh();
}

GitPane::~GitPane()
{
    // A worker may still be running at shutdown; detach and leak the op node
    // so the thread never writes into freed memory.
    if (op_)
    {
        if (op_->worker.joinable())
        {
            op_->worker.detach();
        }
        op_.release();
    }
}

void GitPane::setWorkDir(const std::wstring& dir)
{
    workDir_ = dir;
    lastStatusTime_ = 0;
    showOpLog_ = false;
}

void GitPane::showNotice(const std::wstring& msg)
{
    notice_ = msg;
    noticeTime_ = ::GetTickCount64();
}

// ---------------------------------------------------------------------------
// Data loading
// ---------------------------------------------------------------------------
void GitPane::refreshStatus()
{
    if (workDir_.empty())
    {
        return;
    }
    git::GitStatus s = git_.status(workDir_);
    status_ = s;
    isRepo_ = s.isRepo;
    if (!isRepo_)
    {
        files_.clear();
        diff_.clear();
        branches_.clear();
        commits_.clear();
        remotes_.clear();
        return;
    }
    files_ = s.fileEntries;
    if (selectedFile_ >= (int)files_.size())
    {
        selectedFile_ = (int)files_.size() - 1;
    }
    if (selectedFile_ < 0)
    {
        selectedFile_ = 0;
    }
    // Refresh the diff if it went stale for the currently selected file.
    if (!files_.empty() && (!diff_.empty() || diffLoading_))
    {
        const auto& f = files_[selectedFile_];
        if (diffFile_ != f.path || diffIsCached_ != f.staged || f.untracked)
        {
            diff_.clear();
            loadDiff();
        }
    }
}

void GitPane::loadFiles() { refreshStatus(); }

void GitPane::loadDiff()
{
    if (!isRepo_ || files_.empty() || op_)
    {
        return;
    }
    if (selectedFile_ >= (int)files_.size())
    {
        selectedFile_ = (int)files_.size() - 1;
    }
    if (selectedFile_ < 0)
    {
        return;
    }
    const auto& f = files_[selectedFile_];
    diffFile_ = f.path;
    diffIsCached_ = f.staged;
    diffScroll_ = 0;
    diff_.clear();
    if (f.untracked)
    {
        diffLoading_ = false;
        return; // info view shown in drawChanges
    }

    std::vector<std::wstring> args{ L"diff", L"--no-color", L"--unified=3" };
    if (f.staged)
    {
        args.push_back(L"--cached");
    }
    args.push_back(L"--");
    args.push_back(f.path);

    auto op = std::make_unique<AsyncOp>();
    op->section = Section::Changes;
    op->label = L"Diff";
    op->workDir = workDir_;
    op->args = std::move(args);
    op->extraPath = f.path;
    diffLoading_ = true;
    startOp(std::move(op));
}

void GitPane::loadBranches()
{
    if (!isRepo_ || op_)
    {
        return;
    }
    auto op = std::make_unique<AsyncOp>();
    op->section = Section::Branches;
    op->label = L"Branches";
    op->workDir = workDir_;
    op->args = { L"for-each-ref", L"--format=%(objectname:short)%00%(refname:short)%00%(upstream:short)%00%(upstream:track)",
                 L"refs/heads", L"refs/remotes" };
    startOp(std::move(op));
}

void GitPane::loadHistory()
{
    if (!isRepo_ || op_)
    {
        return;
    }
    auto op = std::make_unique<AsyncOp>();
    op->section = Section::History;
    op->label = L"History";
    op->workDir = workDir_;
    op->args = { L"log", L"--no-color", L"--date=iso", L"--pretty=format:%H%x1f%h%x1f%s%x1f%b%x1f%an%x1f%ae%x1f%ad%x1f%ar%x1f%D",
                 L"-n", L"40" };
    startOp(std::move(op));
}

void GitPane::loadCommitDetail()
{
    if (!isRepo_ || commits_.empty() || op_)
    {
        return;
    }
    if (selectedCommit_ >= (int)commits_.size())
    {
        selectedCommit_ = (int)commits_.size() - 1;
    }
    const auto& c = commits_[selectedCommit_];
    auto op = std::make_unique<AsyncOp>();
    op->section = Section::History;
    op->label = L"Commit detail";
    op->workDir = workDir_;
    op->extraPath = c.hash;
    op->extraA = L"commit-detail";
    commitDetailLoading_ = true;
    startOp(std::move(op));
}

void GitPane::loadGraph()
{
    if (!isRepo_ || op_)
    {
        return;
    }
    auto op = std::make_unique<AsyncOp>();
    op->section = Section::Graph;
    op->label = L"Graph";
    op->workDir = workDir_;
    op->args = { L"log", L"--graph", L"--oneline", L"--decorate", L"--all", L"-n", L"120" };
    op->extraA = L"graph";
    startOp(std::move(op));
}

void GitPane::loadRemotes()
{
    if (!isRepo_ || op_)
    {
        return;
    }
    auto op = std::make_unique<AsyncOp>();
    op->section = Section::Remotes;
    op->label = L"Remotes";
    op->workDir = workDir_;
    op->args = { L"remote" };
    startOp(std::move(op));
}

void GitPane::loadGitHubMeta()
{
    if (op_)
    {
        return;
    }
    ghLoading_ = true;
    runGhOp(L"user");
}

void GitPane::reloadAfterOp() { refreshStatus(); }

void GitPane::setDiffError(const std::wstring& msg)
{
    showNotice(msg);
    diff_.clear();
}

// ---------------------------------------------------------------------------
// Async operation runner / apply
// ---------------------------------------------------------------------------
void GitPane::startOp(std::unique_ptr<AsyncOp> op)
{
    if (op_)
    {
        showNotice(L"Operation in progress: " + op_->label);
        return;
    }
    op_ = std::move(op);
    AsyncOp* p = op_.get();
    std::wstring wd = p->workDir.empty() ? workDir_ : p->workDir;
    std::vector<std::wstring> args = p->args;
    std::wstring extraPath = p->extraPath;
    std::wstring extraA = p->extraA;
    bool github = p->github;
    bool ghPrs = p->ghPrs;
    std::wstring ghRepo = p->ghRepo;
    std::wstring ghAct = p->ghAction;
    std::wstring g1 = p->ghExtra1, g2 = p->ghExtra2, g3 = p->ghExtra3;
    bool commitDetail = (extraA == L"commit-detail");

    p->worker = std::thread([p, wd, args, extraPath, extraA, github, ghPrs, ghRepo, ghAct,
                             g1, g2, g3, commitDetail]() {
        if (github)
        {
            github::GitHub gh;
            gh.setToken(github::GitHub::loadToken());
            if (ghAct == L"user")
            {
                p->ghUser = gh.user();
                p->ok = !p->ghUser.login.empty();
            }
            else if (ghAct == L"repos")
            {
                p->ghRepos = gh.repos();
                p->ok = true;
            }
            else if (ghAct == L"repoData")
            {
                p->ghPRs = gh.pulls(ghRepo);
                p->ghIssues = gh.issues(ghRepo, false);
                p->ok = true;
            }
            else if (ghAct == L"createPR")
            {
                std::wstring url;
                p->ok = gh.createPR(ghRepo, g1, g2, g3, extraA, url);
                p->ghOut = url;
            }
            else if (ghAct == L"createIssue")
            {
                std::wstring url;
                p->ok = gh.createIssue(ghRepo, g1, g2, url);
            }
            p->ghError = gh.lastError();
            p->finished = true;
            return;
        }

        git::Git g;
        if (commitDetail)
        {
            p->result = g.show(wd, extraPath);
            p->diffLines = git::Git::parseDiff(g.showText(wd, extraPath));
        }
        else if (!args.empty() && args[0] == L"for-each-ref")
        {
            // For-each-ref with NUL separators cannot round-trip through the
            // quoting used by run(); use the simple line-based variant.
            p->result = g.runSimple(wd, L"for-each-ref --format=%(objectname:short)%00%(refname:short)%00%(upstream:short)%00%(upstream:track) refs/heads refs/remotes");
        }
        else
        {
            p->result = g.run(wd, args);
            if (!args.empty() && args[0] == L"diff")
            {
                p->diffLines = git::Git::parseDiff(p->result.stdoutText);
            }
        }
        p->ok = p->result.ok;
        p->finished = true;
    });
}

void GitPane::runGitOp(const std::vector<std::wstring>& args, const std::wstring& label,
                       std::wstring extraPath)
{
    if (op_)
    {
        showNotice(L"Operation in progress: " + op_->label);
        return;
    }
    auto op = std::make_unique<AsyncOp>();
    op->section = section_;
    op->label = label;
    op->workDir = workDir_;
    op->args = args;
    op->extraPath = extraPath;
    startOp(std::move(op));
}

void GitPane::runGhOp(const std::wstring& action, const std::wstring& repo,
                      const std::wstring& a, const std::wstring& b,
                      const std::wstring& c, const std::wstring& d)
{
    if (op_)
    {
        showNotice(L"Operation in progress: " + op_->label);
        return;
    }
    auto op = std::make_unique<AsyncOp>();
    op->section = Section::GitHub;
    op->github = true;
    op->label = L"GitHub";
    op->workDir = workDir_;
    op->ghAction = action;
    op->ghRepo = repo;
    op->ghExtra1 = a;
    op->ghExtra2 = b;
    op->ghExtra3 = c;
    op->extraA = d; // optional body / extra field
    startOp(std::move(op));
}

std::wstring GitPane::currentBranchName() const
{
    return status_.branch.find(L'(', 0) == 0 ? L"" : status_.branch;
}

namespace
{
std::vector<std::wstring> splitAllLines(const std::wstring& text, size_t maxLines)
{
    std::vector<std::wstring> out;
    size_t pos = 0;
    while (pos <= text.size() && out.size() < maxLines)
    {
        size_t nl = text.find(L'\n', pos);
        if (nl == std::wstring::npos)
        {
            nl = text.size();
        }
        std::wstring line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }
        if (!line.empty() || true)
        {
            out.push_back(line);
        }
        pos = nl + 1;
    }
    return out;
}
} // namespace

static int parseCommitRecords(const std::wstring& text,
                              std::vector<git::GitCommit>& out)
{
    (void)out;
    int count = 0;
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t nz = text.find(L'\n', pos);
        if (nz == std::wstring::npos)
        {
            nz = text.size();
        }
        std::wstring record = text.substr(pos, nz - pos);
        pos = nz + 1;
        // Fields separated by 0x1f.
        std::vector<std::wstring> f;
        size_t fp = 0;
        while (fp <= record.size())
        {
            size_t fs = record.find(0x1f, fp);
            if (fs == std::wstring::npos)
            {
                fs = record.size();
            }
            f.push_back(record.substr(fp, fs - fp));
            fp = fs + 1;
        }
        if (f.size() < 9)
        {
            continue;
        }
        git::GitCommit c;
        c.hash = f[0];
        c.shortHash = f[1];
        c.subject = f[2];
        c.body = f[3];
        c.author = f[4];
        c.authorEmail = f[5];
        c.date = f[6];
        c.relativeDate = f[7];
        c.branches = f[8];
        c.isHead = c.branches.find(L"HEAD -> ") == 0;
        out.push_back(std::move(c));
        ++count;
    }
    return count;
}

void GitPane::applyOp()
{
    if (!op_ || !op_->finished)
    {
        return;
    }
    if (op_->worker.joinable())
    {
        op_->worker.join();
    }
    std::unique_ptr<AsyncOp> op = std::move(op_);

    // ------------------------------------------------------------------ github
    if (op->github)
    {
        if (op->ghAction == L"user")
        {
            ghUser_ = op->ghUser;
            ghConnected_ = !op->ghUser.login.empty();
            ghLoading_ = false;
            if (ghConnected_)
            {
                ghAccountInfo_ = ghUser_.name.empty()
                                     ? ghUser_.login
                                     : ghUser_.name + L"  @" + ghUser_.login;
                showNotice(L"Connected to GitHub");
                ghRepos_.clear();
                ghRepoIdx_ = -1;
                ghRepoSelected_.clear();
                runGhOp(L"repos");
            }
            else
            {
                showNotice(L"GitHub: " + op->ghError);
            }
        }
        else if (op->ghAction == L"repos")
        {
            ghRepos_ = op->ghRepos;
            ghLoading_ = false;
            if (ghRepos_.empty())
            {
                showNotice(op->ghError.empty() ? L"No repositories found"
                                               : L"GitHub: " + op->ghError);
            }
            else if (ghRepoIdx_ >= (int)ghRepos_.size())
            {
                ghRepoIdx_ = 0;
            }
        }
        else if (op->ghAction == L"repoData")
        {
            ghPRs_ = op->ghPRs;
            ghIssues_ = op->ghIssues;
            ghLoading_ = false;
            if (!op->ghError.empty())
            {
                showNotice(L"GitHub: " + op->ghError);
            }
        }
        else if (op->ghAction == L"createPR")
        {
            ghLoading_ = false;
            showNotice(op->ok ? L"Pull request created" : L"PR failed: " + op->ghError);
            if (op->ok)
            {
                ghPRs_.clear();
                ghIssues_.clear();
                runGhOp(L"repoData", ghRepoSelected_);
            }
        }
        else if (op->ghAction == L"createIssue")
        {
            ghLoading_ = false;
            showNotice(op->ok ? L"Issue created" : L"Issue failed: " + op->ghError);
            if (op->ok)
            {
                ghIssues_.clear();
                runGhOp(L"repoData", ghRepoSelected_);
            }
        }
        return;
    }

    // ---------------------------------------------------------------- git
    const std::wstring cmd = op->args.empty() ? L"" : op->args[0];
    const bool ok = op->ok;

    // Remember output for status messages.
    lastOpLog_.clear();
    if (!op->result.stdoutText.empty() && cmd != L"diff")
    {
        lastOpLog_ = splitAllLines(op->result.stdoutText, 30);
    }

    // Limited-memory safety for the log parser against unusual git output.
    // (commits expected only from "log" ops below).

    if (cmd == L"diff")
    {
        diff_ = op->diffLines;
        diffLoading_ = false;
        // drop hitboxes from a previous layout; content will repaint
        hitBoxes_.clear();
        return;
    }

    if (op->extraA == L"commit-detail")
    {
        commitDetail_ = op->result.stdoutText;
        commitDiff_ = op->diffLines;
        commitDetailLoading_ = false;
        return;
    }

    if (op->extraA == L"graph")
    {
        graph_ = op->result.stdoutText;
        graphScroll_ = 0;
        return;
    }

    if (cmd == L"log")
    {
        commits_.clear();
        parseCommitRecords(op->result.stdoutText, commits_);
        if (selectedCommit_ >= (int)commits_.size())
        {
            selectedCommit_ = (int)commits_.size() - 1;
        }
        if (selectedCommit_ < 0)
        {
            selectedCommit_ = 0;
        }
        commitDetail_.clear();
        commitDiff_.clear();
        if (!commits_.empty())
        {
            loadCommitDetail();
        }
        return;
    }

    if (cmd == L"for-each-ref")
    {
        // Left as-is: branch/remote data filled below through reloadBranches.
        op->ok = true;
        git::Git g;
        // Re-load via the dedicated query that feeds the UI lists.
        branches_ = g.allBranches(workDir_);
        if (selectedBranch_ >= (int)branches_.size())
        {
            selectedBranch_ = (int)branches_.size() - 1;
        }
        if (selectedBranch_ < 0)
        {
            selectedBranch_ = 0;
        }
        return;
    }

    if (cmd == L"remote")
    {
        git::Git g;
        remotes_ = g.remotes(workDir_);
        if (selectedRemote_ >= (int)remotes_.size())
        {
            selectedRemote_ = (int)remotes_.size() - 1;
        }
        if (selectedRemote_ < 0)
        {
            selectedRemote_ = 0;
        }
    }

    if (cmd == L"graph")
    {
        (void)ok;
    }

    // Status message for the hints bar.
    std::wstring msg;
    if (ok)
    {
        if (cmd == L"push")
        {
            msg = L"Push complete";
            refreshStatus();
            loadHistory();
            if (!status_.remoteUrl.empty())
            {
                msg += L"  \u27a4  " + status_.remoteUrl;
            }
            else if (!status_.remoteName.empty())
            {
                msg += L"  \u27a4  " + status_.remoteName;
            }
        }
        else if (cmd == L"pull")
        {
            msg = L"Pull complete";
            refreshStatus();
        }
        else if (cmd == L"fetch")
        {
            msg = L"Fetch complete";
            refreshStatus();
        }
        else if (cmd == L"commit")
        {
            commitMsg_.clear();
            commitDesc_.clear();
            commitCursor_ = 0;
            refreshStatus();
            if (op->extraB == L"push")
            {
                if (status_.detachedHead)
                {
                    // Commit landed on a detached HEAD: no branch ref moved, so a
                    // plain `git push` would be a silent no-op ("up-to-date").
                    showNotice(L"Commit created on detached HEAD - push skipped, create a branch in Branches and push again");
                    loadHistory();
                }
                else
                {
                    // Start the push FIRST: loadHistory() would occupy the single
                    // async-op slot and silently swallow the push.
                    showNotice(L"Commit created - pushing...");
                    runGitOp({ L"push" }, L"Pushing...");
                }
            }
            else
            {
                showNotice(L"Commit created");
                loadHistory();
            }
            return;
        }
        else if (cmd == L"add" || cmd == L"reset" || cmd == L"restore" ||
                 cmd == L"checkout" || cmd == L"clean")
        {
            msg = L"Changes updated";
            refreshStatus();
            if (!op->extraPath.empty() && !diff_.empty())
            {
                loadDiff();
            }
            return;
        }
        else if (cmd == L"branch")
        {
            msg = L"Branch updated";
            refreshStatus();
            loadBranches();
            return;
        }
        else if (cmd == L"merge")
        {
            msg = L"Merge complete";
            refreshStatus();
            loadHistory();
            return;
        }
        else if (cmd == L"revert")
        {
            msg = L"Commit reverted";
            refreshStatus();
            loadHistory();
            return;
        }
        else if (cmd == L"stash")
        {
            msg = L"Stash updated";
            refreshStatus();
            return;
        }
        else if (cmd == L"clone")
        {
            if (op->args.size() >= 3)
            {
                std::wstring target = op->args[2];
                if (!fs::path(target).is_absolute())
                {
                    target = (fs::path(workDir_) / target).wstring();
                }
                workDir_ = target;
                remotes_.clear();
                branches_.clear();
                commits_.clear();
            }
            msg = L"Clone complete";
            refreshStatus();
            loadRemotes();
            return;
        }
        else
        {
            msg = L"Done: " + op->label;
            refreshStatus();
        }
    }
    else
    {
        std::wstring err;
        if (!lastOpLog_.empty())
        {
            for (auto it = lastOpLog_.rbegin(); it != lastOpLog_.rend(); ++it)
            {
                if (!stringutils::trim(*it).empty())
                {
                    err = *it;
                    break;
                }
            }
        }
        if (cmd == L"push")
        {
            const std::wstring& out = op->result.stdoutText;
            if (out.find(L"no upstream") != std::wstring::npos)
            {
                showNotice(L"No upstream branch - pushing with -u");
                runGitOp({ L"push", L"-u", L"origin", L"HEAD" }, L"Pushing...");
                return;
            }
            if (out.find(L"No configured push destination") != std::wstring::npos ||
                out.find(L"does not appear to be a git repository") != std::wstring::npos ||
                out.find(L"Could not read from remote repository") != std::wstring::npos)
            {
                // No usable origin: fall back to the GitHub repo selected in the
                // GitHub section (https URL, no token stored in .git/config).
                if (ghConnected_ && !ghRepoSelected_.empty())
                {
                    showNotice(L"No origin - adding " + ghRepoSelected_ + L" and pushing");
                    git::Git g;
                    auto ar = g.run(workDir_, { L"remote", L"add", L"origin",
                                                L"https://github.com/" + ghRepoSelected_ + L".git" });
                    if (!ar.ok)
                    {
                        g.run(workDir_, { L"remote", L"set-url", L"origin",
                                          L"https://github.com/" + ghRepoSelected_ + L".git" });
                    }
                    refreshStatus();
                    runGitOp({ L"push", L"-u", L"origin", L"HEAD" }, L"Pushing...");
                    return;
                }
            }
        }
        msg = op->label + L" failed";
        if (!err.empty())
        {
            msg += L": " + clipRight(err, 50);
        }
        refreshStatus();
    }
    showNotice(msg);
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------
void GitPane::openDialog(DialogKind kind)
{
    dialog_ = kind;
    dialogFieldFocus_ = 0;
    dialogFieldCursor_ = 0;
    dialogButtonFocus_ = 0;
    dialogOptionIdx_ = 0;
    dialogOptionScroll_ = 0;
    dialogOptions_.clear();
    for (int i = 0; i < 4; ++i)
    {
        dialogFields_[i].clear();
        dialogFieldTitles_[i].clear();
    }

    switch (kind)
    {
    case DialogKind::Confirm:
        dialogFieldCount_ = 0;
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"";
        break;
    case DialogKind::NewBranch:
        dialogFieldCount_ = 2;
        dialogFieldTitles_[0] = L"Branch name";
        dialogFieldTitles_[1] = L"Start point";
        if (!status_.branch.empty() && status_.branch.find(L'(') != 0)
        {
            dialogFields_[1] = status_.branch;
        }
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Create";
        dialogFieldFocus_ = 0;
        break;
    case DialogKind::DeleteBranch:
        dialogFieldCount_ = 1;
        dialogFieldTitles_[0] = L"Branch";
        if (selectedBranch_ >= 0 && selectedBranch_ < (int)branches_.size())
        {
            dialogFields_[0] = branches_[selectedBranch_].name;
        }
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Delete";
        break;
    case DialogKind::RenameBranch:
        dialogFieldCount_ = 1;
        dialogFieldTitles_[0] = L"New name";
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Rename";
        break;
    case DialogKind::MergeBranch:
        dialogFieldCount_ = 0;
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Merge";
        for (const auto& b : branches_)
        {
            if (!b.isRemote && b.name != currentBranchName())
            {
                dialogOptions_.push_back(b.name);
            }
        }
        break;
    case DialogKind::Stash:
        dialogFieldCount_ = 1;
        dialogFieldTitles_[0] = L"Message";
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Stash";
        break;
    case DialogKind::PopStash:
        dialogFieldCount_ = 0;
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Pop";
        break;
    case DialogKind::CloneRepo:
        dialogFieldCount_ = 2;
        dialogFieldTitles_[0] = L"URL";
        dialogFieldTitles_[1] = L"Directory (auto)";
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Clone";
        break;
    case DialogKind::AddRemote:
        dialogFieldCount_ = 2;
        dialogFieldTitles_[0] = L"Name";
        dialogFieldTitles_[1] = L"URL";
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Add";
        break;
    case DialogKind::RemoveRemote:
        dialogFieldCount_ = 1;
        dialogFieldTitles_[0] = L"Remote";
        if (selectedRemote_ >= 0 && selectedRemote_ < (int)remotes_.size())
        {
            dialogFields_[0] = remotes_[selectedRemote_].name;
        }
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Remove";
        break;
    case DialogKind::GithubToken:
        dialogFieldCount_ = 1;
        dialogFieldTitles_[0] = L"Personal access token";
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Connect";
        break;
    case DialogKind::CreatePR:
        dialogFieldCount_ = 4;
        dialogFieldTitles_[0] = L"Base branch";
        dialogFieldTitles_[1] = L"Head branch";
        dialogFieldTitles_[2] = L"Title";
        dialogFieldTitles_[3] = L"Description";
        if (!ghRepos_.empty() && ghRepoIdx_ >= 0 && ghRepoIdx_ < (int)ghRepos_.size())
        {
            dialogFields_[0] = ghRepos_[ghRepoIdx_].defaultBranch;
        }
        dialogFields_[1] = currentBranchName();
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Create PR";
        break;
    case DialogKind::CreateIssue:
        dialogFieldCount_ = 2;
        dialogFieldTitles_[0] = L"Title";
        dialogFieldTitles_[1] = L"Description";
        dialogButtons_ = 2;
        dialogButtonTitle_ = L"Create";
        break;
    case DialogKind::None:
        break;
    }
}

void GitPane::closeDialog()
{
    dialog_ = DialogKind::None;
    dialogOptions_.clear();
}

void GitPane::applyDialogOK()
{
    if (dialog_ == DialogKind::None)
    {
        return;
    }
    const DialogKind kind = dialog_;
    closeDialog();

    switch (kind)
    {
    case DialogKind::Confirm:
    {
        std::wstring action = dialogFields_[0];
        if (stringutils::startsWith(action, L"DISCARD:"))
        {
            std::wstring path = action.substr(8);
            bool untracked = false;
            for (const auto& f : files_)
            {
                if (f.path == path)
                {
                    untracked = f.untracked;
                    break;
                }
            }
            if (untracked)
            {
                runGitOp({ L"clean", L"-f", L"--", path }, L"Discarding...", path);
            }
            else
            {
                runGitOp({ L"checkout", L"--", path }, L"Discarding...", path);
            }
        }
        else if (stringutils::startsWith(action, L"REVERT:"))
        {
            runGitOp({ L"revert", L"--no-edit", action.substr(7) }, L"Reverting...");
        }
        else if (stringutils::startsWith(action, L"RESET:"))
        {
            runGitOp({ L"reset", L"--mixed", action.substr(6) }, L"Resetting...");
        }
        else if (stringutils::startsWith(action, L"DELBR:"))
        {
            runGitOp({ L"branch", L"-d", action.substr(6) }, L"Deleting branch...");
        }
        else if (stringutils::startsWith(action, L"DELREM:"))
        {
            runGitOp({ L"remote", L"remove", action.substr(7) }, L"Removing remote...");
        }
        break;
    }
    case DialogKind::NewBranch:
    {
        std::wstring name = stringutils::trim(dialogFields_[0]);
        if (name.empty())
        {
            showNotice(L"Branch name is required");
            return;
        }
        std::vector<std::wstring> args{ L"checkout", L"-b", name };
        std::wstring start = stringutils::trim(dialogFields_[1]);
        if (!start.empty())
        {
            args.push_back(start);
        }
        runGitOp(args, L"Creating branch...");
        break;
    }
    case DialogKind::DeleteBranch:
        runGitOp({ L"branch", L"-d", stringutils::trim(dialogFields_[0]) },
                 L"Deleting branch...");
        break;
    case DialogKind::RenameBranch:
    {
        std::wstring oldName = branches_[selectedBranch_].name;
        runGitOp({ L"branch", L"-m", oldName, stringutils::trim(dialogFields_[0]) },
                 L"Renaming branch...");
        break;
    }
    case DialogKind::MergeBranch:
        if (dialogOptionIdx_ >= 0 && dialogOptionIdx_ < (int)dialogOptions_.size())
        {
            runGitOp({ L"merge", dialogOptions_[dialogOptionIdx_] }, L"Merging...");
        }
        break;
    case DialogKind::Stash:
    {
        std::wstring msg = stringutils::trim(dialogFields_[0]);
        if (msg.empty())
        {
            runGitOp({ L"stash", L"push", L"-u" }, L"Stashing...");
        }
        else
        {
            runGitOp({ L"stash", L"push", L"-u", L"-m", msg }, L"Stashing...");
        }
        break;
    }
    case DialogKind::PopStash:
        runGitOp({ L"stash", L"pop" }, L"Popping stash...");
        break;
    case DialogKind::CloneRepo:
    {
        std::wstring url = stringutils::trim(dialogFields_[0]);
        if (url.empty())
        {
            showNotice(L"Clone URL is required");
            return;
        }
        std::vector<std::wstring> args{ L"clone", url };
        std::wstring dir = stringutils::trim(dialogFields_[1]);
        if (dir.empty())
        {
            // default: repo name from URL
            std::wstring base = url;
            size_t slash = base.find_last_of(L"/\\");
            if (slash != std::wstring::npos)
            {
                base = base.substr(slash + 1);
            }
            if (stringutils::endsWith(base, L".git"))
            {
                base = base.substr(0, base.size() - 4);
            }
            if (!base.empty())
            {
                args.push_back(base);
            }
        }
        else
        {
            args.push_back(dir);
        }
        runGitOp(args, L"Cloning...");
        break;
    }
    case DialogKind::AddRemote:
    {
        std::wstring name = stringutils::trim(dialogFields_[0]);
        std::wstring url = stringutils::trim(dialogFields_[1]);
        if (name.empty() || url.empty())
        {
            showNotice(L"Name and URL are required");
            return;
        }
        runGitOp({ L"remote", L"add", name, url }, L"Adding remote...");
        break;
    }
    case DialogKind::RemoveRemote:
        runGitOp({ L"remote", L"remove", stringutils::trim(dialogFields_[0]) },
                 L"Removing remote...");
        break;
    case DialogKind::GithubToken:
    {
        std::wstring token = stringutils::trim(dialogFields_[0]);
        if (token.empty())
        {
            showNotice(L"Token is required");
            return;
        }
        github::GitHub::saveToken(token);
        ghToken_ = token;
        ghConnected_ = true;
        showNotice(L"Token saved - connecting...");
        runGhOp(L"user");
        break;
    }
    case DialogKind::CreatePR:
    {
        std::wstring repo = ghRepoSelected_;
        std::wstring base = stringutils::trim(dialogFields_[0]);
        std::wstring head = stringutils::trim(dialogFields_[1]);
        std::wstring title = stringutils::trim(dialogFields_[2]);
        std::wstring body = stringutils::trim(dialogFields_[3]);
        if (repo.empty() || base.empty() || head.empty() || title.empty())
        {
            showNotice(L"Base, head and title are required");
            return;
        }
        runGhOp(L"createPR", repo, base, head, title, body);
        break;
    }
    case DialogKind::CreateIssue:
    {
        std::wstring repo = ghRepoSelected_;
        std::wstring title = stringutils::trim(dialogFields_[0]);
        std::wstring body = stringutils::trim(dialogFields_[1]);
        if (repo.empty() || title.empty())
        {
            showNotice(L"Title is required");
            return;
        }
        runGhOp(L"createIssue", repo, title, body);
        break;
    }
    case DialogKind::None:
        break;
    }
}

// ---------------------------------------------------------------------------
// Keyboard helpers
// ---------------------------------------------------------------------------
bool GitPane::editField(std::wstring& buf, int& cur, const KeyEvent& key)
{
    if (key.isPrint())
    {
        if (key.ch == L'\r')
        {
            return false;
        }
        buf.insert(size_t(cur), 1, key.ch);
        ++cur;
        return true;
    }
    switch (key.key)
    {
    case Key::Left:
        if (cur > 0) --cur;
        return true;
    case Key::Right:
        if (cur < (int)buf.size()) ++cur;
        return true;
    case Key::Home:
        cur = 0;
        return true;
    case Key::End:
        cur = (int)buf.size();
        return true;
    case Key::Backspace:
        if (cur > 0)
        {
            buf.erase((size_t)cur - 1, 1);
            --cur;
        }
        return true;
    case Key::Delete:
        if (cur < (int)buf.size())
        {
            buf.erase((size_t)cur, 1);
        }
        return true;
    default:
        break;
    }
    return false;
}

void GitPane::keyDialog(const KeyEvent& key)
{
    if (key.key == Key::Escape || key.key == Key::F5)
    {
        closeDialog();
        return;
    }
    if (dialog_ == DialogKind::MergeBranch)
    {
        if (!dialogOptions_.empty())
        {
            if (key.key == Key::Up || key.key == Key::Down)
            {
                dialogOptionIdx_ += (key.key == Key::Up ? -1 : 1);
                dialogOptionIdx_ = std::max(0, std::min((int)dialogOptions_.size() - 1, dialogOptionIdx_));
            }
            else if (key.key == Key::Enter)
            {
                applyDialogOK();
            }
            return;
        }
    }
    int total = dialogFieldCount_ + dialogButtons_;
    if (key.key == Key::Tab || key.key == Key::Down)
    {
        dialogFieldFocus_ = (dialogFieldFocus_ + 1) % std::max(1, total);
        return;
    }
    if (key.key == Key::Up)
    {
        dialogFieldFocus_ = (dialogFieldFocus_ - 1 + std::max(1, total)) % std::max(1, total);
        return;
    }
    if (key.key == Key::Enter)
    {
        if (dialogFieldFocus_ >= dialogFieldCount_)
        {
            if (dialogFieldFocus_ - dialogFieldCount_ == 0)
            {
                applyDialogOK();
            }
            else
            {
                closeDialog();
            }
        }
        else
        {
            applyDialogOK();
        }
        return;
    }
    if (dialogFieldFocus_ < dialogFieldCount_)
    {
        if (key.key == Key::Left || key.key == Key::Right)
        {
            dialogFieldCursor_ += (key.key == Key::Left ? -1 : 1);
            if (dialogFieldCursor_ < 0) dialogFieldCursor_ = 0;
            if (dialogFieldCursor_ > (int)dialogFields_[dialogFieldFocus_].size())
            {
                dialogFieldCursor_ = (int)dialogFields_[dialogFieldFocus_].size();
            }
            return;
        }
        editField(dialogFields_[dialogFieldFocus_], dialogFieldCursor_, key);
    }
}

// ---------------------------------------------------------------------------
// Refresh (periodic, from the app loop)
// ---------------------------------------------------------------------------
void GitPane::refresh()
{
    if (op_)
    {
        if (op_->finished)
        {
            applyOp();
        }
        return;
    }
    ULONGLONG now = ::GetTickCount64();
    if (now - lastStatusTime_ < 900)
    {
        return;
    }
    lastStatusTime_ = now;
    refreshStatus();
}

// ---------------------------------------------------------------------------
// Keyboard entry points per section
// ---------------------------------------------------------------------------
void GitPane::keyOverview(const KeyEvent& key)
{
    switch (key.key)
    {
    case Key::O:
        if (!status_.remoteUrl.empty())
        {
            openUrl(remoteBrowserUrl());
        }
        return;
    case Key::F5:
    case Key::R:
        lastStatusTime_ = 0;
        refreshStatus();
        return;
    case Key::F:
        runGitOp({ L"fetch" }, L"Fetching...");
        return;
    case Key::P:
        runGitOp({ L"push" }, L"Pushing...");
        return;
    case Key::L:
        runGitOp({ L"pull" }, L"Pulling...");
        return;
    case Key::S:
        openDialog(DialogKind::Stash);
        return;
    case Key::X:
        openDialog(DialogKind::PopStash);
        return;
    case Key::C:
        openDialog(DialogKind::CloneRepo);
        return;
    default:
        break;
    }
}

void GitPane::commitAndPush()
{
    std::wstring msg = stringutils::trim(commitMsg_);
    if (msg.empty())
    {
        showNotice(L"Commit message is empty");
        return;
    }
    std::vector<std::wstring> args{ L"commit", L"-m", msg };
    std::wstring desc = stringutils::trim(commitDesc_);
    if (!desc.empty())
    {
        args.push_back(L"-m");
        args.push_back(desc);
    }
    auto op = std::make_unique<AsyncOp>();
    op->section = Section::Changes;
    op->label = L"Committing...";
    op->workDir = workDir_;
    op->args = std::move(args);
    op->extraB = L"push";
    startOp(std::move(op));
}

void GitPane::commitOnly()
{
    std::wstring msg = stringutils::trim(commitMsg_);
    if (msg.empty())
    {
        showNotice(L"Commit message is empty");
        return;
    }
    std::vector<std::wstring> args{ L"commit", L"-m", msg };
    std::wstring desc = stringutils::trim(commitDesc_);
    if (!desc.empty())
    {
        args.push_back(L"-m");
        args.push_back(desc);
    }
    runGitOp(std::move(args), L"Committing...");
}

void GitPane::stageToggleSelected()
{
    if (selectedFile_ < 0 || selectedFile_ >= (int)files_.size())
    {
        return;
    }
    const auto& f = files_[selectedFile_];
    std::wstring path = f.path;
    if (f.staged)
    {
        runGitOp({ L"reset", L"-q", L"HEAD", L"--", path }, L"Unstaging...", path);
    }
    else if (f.untracked)
    {
        runGitOp({ L"add", L"--", path }, L"Staging...", path);
    }
    else
    {
        runGitOp({ L"add", L"--", path }, L"Staging...", path);
    }
}

void GitPane::discardSelected()
{
    if (selectedFile_ < 0 || selectedFile_ >= (int)files_.size())
    {
        return;
    }
    const auto& f = files_[selectedFile_];
    openDialog(DialogKind::Confirm);
    dialogTitle_ = L"Discard changes to " + f.path + L"?";
    dialogFields_[0] = L"DISCARD:" + f.path;
}

void GitPane::keyChanges(const KeyEvent& key)
{
    if (key.key == Key::Escape)
    {
        focus_ = Focus::Files;
        return;
    }
    if (focus_ == Focus::CommitMsg || focus_ == Focus::CommitDesc)
    {
        std::wstring& buf = (focus_ == Focus::CommitMsg) ? commitMsg_ : commitDesc_;
        if (editField(buf, commitCursor_, key))
        {
            return;
        }
        if (key.key == Key::Tab)
        {
            focus_ = (focus_ == Focus::CommitMsg) ? Focus::CommitDesc : Focus::Files;
            commitCursor_ = 0;
            return;
        }
        if (key.key == Key::Enter)
        {
            commitOnly();
            return;
        }
        if (key.key == Key::Escape)
        {
            focus_ = Focus::Files;
            return;
        }
        return;
    }
    // focus == Files
    switch (key.key)
    {
    case Key::Up:
        if (selectedFile_ > 0)
        {
            --selectedFile_;
            loadDiff();
        }
        return;
    case Key::Down:
        if (selectedFile_ + 1 < (int)files_.size())
        {
            ++selectedFile_;
            loadDiff();
        }
        return;
    case Key::PageUp:
        selectedFile_ = std::max(0, selectedFile_ - (int)(24));
        loadDiff();
        return;
    case Key::PageDown:
        selectedFile_ = std::min((int)files_.size() - 1, selectedFile_ + 24);
        loadDiff();
        return;
    case Key::Home:
        selectedFile_ = 0;
        loadDiff();
        return;
    case Key::End:
        selectedFile_ = (int)files_.size() - 1;
        loadDiff();
        return;
    case Key::Tab:
        focus_ = Focus::CommitMsg;
        commitCursor_ = (int)commitMsg_.size();
        return;
    case Key::Enter:
    case Key::Space:
        stageToggleSelected();
        return;
    default:
        break;
    }
    if (key.isPrint())
    {
        switch (key.ch)
        {
        case L's':
        case L'u':
            stageToggleSelected();
            return;
        case L'a':
            runGitOp({ L"add", L"-A" }, L"Staging all...");
            return;
        case L'z':
            runGitOp({ L"reset", L"-q", L"HEAD", L"--", L"." }, L"Unstaging all...");
            return;
        case L'd':
            discardSelected();
            return;
        case L'x':
            sideBySide_ = !sideBySide_;
            diffScroll_ = 0;
            return;
        case L'c':
            commitOnly();
            return;
        case L'p':
            commitAndPush();
            return;
        default:
            break;
        }
    }
}

void GitPane::keyBranches(const KeyEvent& key)
{
    switch (key.key)
    {
    case Key::Up:
        if (selectedBranch_ > 0)
        {
            --selectedBranch_;
        }
        return;
    case Key::Down:
        if (selectedBranch_ + 1 < (int)branches_.size())
        {
            ++selectedBranch_;
        }
        return;
    case Key::Enter:
    {
        if (selectedBranch_ < 0 || selectedBranch_ >= (int)branches_.size())
        {
            return;
        }
        const auto& b = branches_[selectedBranch_];
        if (b.isRemote)
        {
            std::wstring local = b.name;
            size_t slash = local.find(L'/');
            if (slash != std::wstring::npos)
            {
                local = local.substr(slash + 1);
            }
            runGitOp({ L"checkout", L"-b", local, b.name }, L"Checking out...");
        }
        else if (!b.isCurrent)
        {
            runGitOp({ L"checkout", b.name }, L"Checking out...");
        }
        return;
    }
    case Key::N:
        openDialog(DialogKind::NewBranch);
        return;
    case Key::R:
        if (selectedBranch_ >= 0 && selectedBranch_ < (int)branches_.size())
        {
            openDialog(DialogKind::RenameBranch);
        }
        return;
    case Key::D:
        if (selectedBranch_ >= 0 && selectedBranch_ < (int)branches_.size() &&
            !branches_[selectedBranch_].isCurrent)
        {
            openDialog(DialogKind::Confirm);
            dialogTitle_ = L"Delete branch " + branches_[selectedBranch_].name + L"?";
            dialogFields_[0] = L"DELBR:" + branches_[selectedBranch_].name;
        }
        return;
    case Key::M:
        openDialog(DialogKind::MergeBranch);
        return;
    case Key::F:
        runGitOp({ L"fetch", L"--all" }, L"Fetching...");
        return;
    default:
        break;
    }
}

void GitPane::keyHistory(const KeyEvent& key)
{
    switch (key.key)
    {
    case Key::Up:
        if (selectedCommit_ > 0)
        {
            --selectedCommit_;
            loadCommitDetail();
        }
        return;
    case Key::Down:
        if (selectedCommit_ + 1 < (int)commits_.size())
        {
            ++selectedCommit_;
            loadCommitDetail();
        }
        return;
    case Key::PageUp:
        selectedCommit_ = std::max(0, selectedCommit_ - 20);
        loadCommitDetail();
        return;
    case Key::PageDown:
        selectedCommit_ = std::min((int)commits_.size() - 1, selectedCommit_ + 20);
        loadCommitDetail();
        return;
    case Key::C:
        if (!commits_.empty() && selectedCommit_ >= 0 && selectedCommit_ < (int)commits_.size())
        {
            setClipboard(commits_[selectedCommit_].hash);
            showNotice(L"Full hash copied");
        }
        return;
    case Key::B:
        if (!commits_.empty())
        {
            openDialog(DialogKind::NewBranch);
            dialogFields_[1] = commits_[selectedCommit_].shortHash;
        }
        return;
    case Key::V:
        if (!commits_.empty())
        {
            openDialog(DialogKind::Confirm);
            dialogTitle_ = L"Revert commit " + commits_[selectedCommit_].shortHash + L"?";
            dialogFields_[0] = L"REVERT:" + commits_[selectedCommit_].hash;
        }
        return;
    case Key::R:
        if (!commits_.empty())
        {
            openDialog(DialogKind::Confirm);
            dialogTitle_ = L"Reset (" + commits_[selectedCommit_].shortHash + L")?";
            dialogFields_[0] = L"RESET:" + commits_[selectedCommit_].hash;
        }
        return;
    case Key::F5:
        lastStatusTime_ = 0;
        loadHistory();
        return;
    default:
        break;
    }
}

void GitPane::keyGraph(const KeyEvent& key)
{
    switch (key.key)
    {
    case Key::Up:
        if (graphScroll_ > 0) --graphScroll_;
        return;
    case Key::Down:
        ++graphScroll_;
        return;
    case Key::F5:
    case Key::R:
        loadGraph();
        return;
    default:
        break;
    }
}

void GitPane::keyRemotes(const KeyEvent& key)
{
    switch (key.key)
    {
    case Key::Up:
        if (selectedRemote_ > 0)
        {
            --selectedRemote_;
        }
        return;
    case Key::Down:
        if (selectedRemote_ + 1 < (int)remotes_.size())
        {
            ++selectedRemote_;
        }
        return;
    case Key::F:
        runGitOp({ L"fetch", L"--all" }, L"Fetching...");
        return;
    case Key::P:
        runGitOp({ L"push" }, L"Pushing...");
        return;
    case Key::L:
        runGitOp({ L"pull" }, L"Pulling...");
        return;
    case Key::A:
        openDialog(DialogKind::AddRemote);
        return;
    case Key::D:
        if (selectedRemote_ >= 0 && selectedRemote_ < (int)remotes_.size())
        {
            openDialog(DialogKind::Confirm);
            dialogTitle_ = L"Remove remote " + remotes_[selectedRemote_].name + L"?";
            dialogFields_[0] = L"DELREM:" + remotes_[selectedRemote_].name;
        }
        return;
    case Key::O:
        if (selectedRemote_ >= 0 && selectedRemote_ < (int)remotes_.size())
        {
            std::wstring url = browserUrl(remotes_[selectedRemote_].url);
            if (!url.empty())
            {
                openUrl(url);
            }
        }
        return;
    default:
        break;
    }
}

void GitPane::keyGitHub(const KeyEvent& key)
{
    switch (key.key)
    {
    case Key::Up:
        if (ghRepoIdx_ > 0)
        {
            --ghRepoIdx_;
            ghRepoSelected_ = ghRepos_[ghRepoIdx_].fullName;
            ghPRs_.clear();
            ghIssues_.clear();
            runGhOp(L"repoData", ghRepoSelected_);
        }
        return;
    case Key::Down:
        if (ghRepoIdx_ + 1 < (int)ghRepos_.size())
        {
            ++ghRepoIdx_;
            ghRepoSelected_ = ghRepos_[ghRepoIdx_].fullName;
            ghPRs_.clear();
            ghIssues_.clear();
            runGhOp(L"repoData", ghRepoSelected_);
        }
        return;
    case Key::C:
        if (!ghConnected_)
        {
            openDialog(DialogKind::GithubToken);
        }
        return;
    case Key::Escape:
        return;
    case Key::O:
        if (ghRepoIdx_ >= 0 && ghRepoIdx_ < (int)ghRepos_.size())
        {
            openUrl(ghRepos_[ghRepoIdx_].htmlUrl);
        }
        return;
    case Key::N:
        if (ghConnected_ && !ghRepoSelected_.empty())
        {
            openDialog(DialogKind::CreatePR);
        }
        return;
    case Key::I:
        if (ghConnected_ && !ghRepoSelected_.empty())
        {
            openDialog(DialogKind::CreateIssue);
        }
        return;
    case Key::F5:
    case Key::R:
        if (ghConnected_)
        {
            runGhOp(L"repos");
        }
        return;
    default:
        break;
    }
}

bool GitPane::onKey(const KeyEvent& key)
{
    if (dialog_ != DialogKind::None)
    {
        keyDialog(key);
        return true;
    }
    if (key.key == Key::F5 && section_ != Section::History)
    {
        showOpLog_ = false;
        lastStatusTime_ = 0;
        if (section_ == Section::GitHub)
        {
            if (ghConnected_ && !op_)
            {
                runGhOp(L"repos");
            }
        }
        else if (section_ == Section::Branches && isRepo_)
        {
            loadBranches();
        }
        else if (section_ == Section::Graph && isRepo_)
        {
            loadGraph();
        }
        else if (section_ == Section::Remotes && isRepo_)
        {
            loadRemotes();
        }
        else if (section_ == Section::History && isRepo_)
        {
            loadHistory();
        }
        else
        {
            refreshStatus();
        }
        return true;
    }

    // Section switching.
    if (key.key == Key::Left || key.key == Key::BracketOpen)
    {
        Section s = section_;
        s = static_cast<Section>((static_cast<int>(s) - 1 + 7) % 7);
        if (section_ != s)
        {
            focus_ = Focus::Files;
            section_ = s;
            switch (section_)
            {
            case Section::Changes: loadFiles(); break;
            case Section::Branches: if (isRepo_) loadBranches(); break;
            case Section::History: if (isRepo_) loadHistory(); break;
            case Section::Graph: if (isRepo_) loadGraph(); break;
            case Section::Remotes: if (isRepo_) loadRemotes(); break;
            case Section::GitHub: loadGitHubMeta(); break;
            case Section::Overview: lastStatusTime_ = 0; refreshStatus(); break;
            }
        }
        return true;
    }
    if (key.key == Key::Right || key.key == Key::BracketClose)
    {
        Section s = section_;
        s = static_cast<Section>((static_cast<int>(s) + 1) % 7);
        if (section_ != s)
        {
            focus_ = Focus::Files;
            section_ = s;
            switch (section_)
            {
            case Section::Changes: loadFiles(); break;
            case Section::Branches: if (isRepo_) loadBranches(); break;
            case Section::History: if (isRepo_) loadHistory(); break;
            case Section::Graph: if (isRepo_) loadGraph(); break;
            case Section::Remotes: if (isRepo_) loadRemotes(); break;
            case Section::GitHub: loadGitHubMeta(); break;
            case Section::Overview: lastStatusTime_ = 0; refreshStatus(); break;
            }
        }
        return true;
    }

    if (op_ && !op_->finished)
    {
        return true; // keep input until the current operation settles
    }

    switch (section_)
    {
    case Section::Overview: keyOverview(key); break;
    case Section::Changes: keyChanges(key); break;
    case Section::Branches: keyBranches(key); break;
    case Section::History: keyHistory(key); break;
    case Section::Graph: keyGraph(key); break;
    case Section::Remotes: keyRemotes(key); break;
    case Section::GitHub: keyGitHub(key); break;
    }
    return true;
}

void GitPane::onMouseWheel(int delta)
{
    if (dialog_ != DialogKind::None)
    {
        return;
    }
    switch (section_)
    {
    case Section::Changes:
        fileScroll_ -= delta;
        if (fileScroll_ < 0) fileScroll_ = 0;
        return;
    case Section::Branches:
        branchScroll_ -= delta;
        if (branchScroll_ < 0) branchScroll_ = 0;
        return;
    case Section::History:
        historyScroll_ -= delta;
        if (historyScroll_ < 0) historyScroll_ = 0;
        return;
    case Section::Graph:
        graphScroll_ += delta;
        if (graphScroll_ < 0) graphScroll_ = 0;
        return;
    case Section::Remotes:
        remoteScroll_ -= delta;
        if (remoteScroll_ < 0) remoteScroll_ = 0;
        return;
    case Section::GitHub:
        ghScroll_ -= delta;
        if (ghScroll_ < 0) ghScroll_ = 0;
        return;
    default:
        return;
    }
}

bool GitPane::onMouseClick(int rowInPane, int colInPane, bool doubleClick)
{
    if (dialog_ != DialogKind::None)
    {
        // hit test on dialog elements
        for (const auto& h : hitBoxes_)
        {
            if (rowInPane == h.row && colInPane >= h.x && colInPane < h.x + h.w)
            {
                if (h.id == L"dbtn")
                {
                    if (h.index == 0)
                    {
                        applyDialogOK();
                    }
                    else
                    {
                        closeDialog();
                    }
                    return true;
                }
                if (h.id == L"dopt")
                {
                    dialogOptionIdx_ = h.index;
                    return true;
                }
                if (h.id == L"dfield")
                {
                    dialogFieldFocus_ = h.index;
                    dialogFieldCursor_ = (int)dialogFields_[h.index].size();
                    return true;
                }
            }
        }
        return true;
    }

    for (const auto& h : hitBoxes_)
    {
        if (rowInPane == h.row && colInPane >= h.x && colInPane < h.x + h.w)
        {
            if (h.id == L"nav")
            {
                Section s = static_cast<Section>(h.index);
                if (section_ != s)
                {
                    focus_ = Focus::Files;
                    section_ = s;
                    switch (section_)
                    {
                    case Section::Changes: loadFiles(); break;
                    case Section::Branches: if (isRepo_) loadBranches(); break;
                    case Section::History: if (isRepo_) loadHistory(); break;
                    case Section::Graph: if (isRepo_) loadGraph(); break;
                    case Section::Remotes: if (isRepo_) loadRemotes(); break;
                    case Section::GitHub: loadGitHubMeta(); break;
                    case Section::Overview: lastStatusTime_ = 0; refreshStatus(); break;
                    }
                }
                return true;
            }
            if (h.id == L"file")
            {
                selectedFile_ = h.index;
                loadDiff();
                if (doubleClick)
                {
                    stageToggleSelected();
                }
                return true;
            }
            if (h.id == L"cmf")
            {
                focus_ = (h.index == 0) ? Focus::CommitMsg : Focus::CommitDesc;
                commitCursor_ = (int)((h.index == 0) ? commitMsg_.size() : commitDesc_.size());
                return true;
            }
            if (h.id == L"btn")
            {
                dispatchButton(h.index);
                return true;
            }
            if (h.id == L"branch")
            {
                selectedBranch_ = h.index;
                return true;
            }
            if (h.id == L"commit")
            {
                selectedCommit_ = h.index;
                loadCommitDetail();
                return true;
            }
            if (h.id == L"remote")
            {
                selectedRemote_ = h.index;
                return true;
            }
            if (h.id == L"ghrepo")
            {
                ghRepoIdx_ = h.index;
                ghRepoSelected_ = ghRepos_[h.index].fullName;
                ghPRs_.clear();
                ghIssues_.clear();
                runGhOp(L"repoData", ghRepoSelected_);
                return true;
            }
            break;
        }
    }
    return true;
}

// Mouse click dispatch for rendered action buttons.
void GitPane::dispatchButton(int index)
{
    switch (section_)
    {
    case Section::Overview:
        switch (index)
        {
        case 0: if (!status_.remoteUrl.empty()) openUrl(remoteBrowserUrl()); break;
        case 1: lastStatusTime_ = 0; refreshStatus(); break;
        case 2: runGitOp({ L"fetch" }, L"Fetching..."); break;
        case 3: runGitOp({ L"push" }, L"Pushing..."); break;
        case 4: runGitOp({ L"pull" }, L"Pulling..."); break;
        default: break;
        }
        break;
    case Section::Changes:
        switch (index)
        {
        case 0: case 1: stageToggleSelected(); break;
        case 2: discardSelected(); break;
        case 3: runGitOp({ L"add", L"-A" }, L"Staging all..."); break;
        case 4: runGitOp({ L"reset", L"-q", L"HEAD", L"--", L"." }, L"Unstaging all..."); break;
        case 5: sideBySide_ = !sideBySide_; diffScroll_ = 0; break;
        case 6: commitOnly(); break;
        case 7: commitAndPush(); break;
        default: break;
        }
        break;
    case Section::Branches:
        switch (index)
        {
        case 0: openDialog(DialogKind::NewBranch); break;
        case 1: keyBranches({ Key::Enter }); break;
        case 2: openDialog(DialogKind::RenameBranch); break;
        case 3: keyBranches({ Key::D }); break;
        case 4: openDialog(DialogKind::MergeBranch); break;
        case 5: runGitOp({ L"fetch", L"--all" }, L"Fetching..."); break;
        default: break;
        }
        break;
    case Section::History:
        switch (index)
        {
        case 0: keyHistory({ Key::C }); break;
        case 1: keyHistory({ Key::B }); break;
        case 2: keyHistory({ Key::V }); break;
        case 3: keyHistory({ Key::R }); break;
        default: break;
        }
        break;
    case Section::Graph:
        if (index == 0)
        {
            loadGraph();
        }
        break;
    case Section::Remotes:
        switch (index)
        {
        case 0: runGitOp({ L"fetch", L"--all" }, L"Fetching..."); break;
        case 1: runGitOp({ L"push" }, L"Pushing..."); break;
        case 2: runGitOp({ L"pull" }, L"Pulling..."); break;
        case 3: openDialog(DialogKind::AddRemote); break;
        case 4: keyRemotes({ Key::D }); break;
        case 5: keyRemotes({ Key::O }); break;
        default: break;
        }
        break;
    case Section::GitHub:
        switch (index)
        {
        case 0: if (!ghConnected_) openDialog(DialogKind::GithubToken); break;
        case 1: keyGitHub({ Key::O }); break;
        case 2: keyGitHub({ Key::N }); break;
        case 3: keyGitHub({ Key::I }); break;
        case 4: keyGitHub({ Key::R }); break;
        case 5: ghConnected_ = false; ghUser_ = {}; ghRepos_.clear(); ghPRs_.clear(); ghIssues_.clear();
                github::GitHub::saveToken(L""); ghRepoSelected_.clear(); break;
        default: break;
        }
        break;
    default:
        break;
    }
}

void GitPane::openUrl(const std::wstring& url)
{
    if (url.empty())
    {
        showNotice(L"No web URL available");
        return;
    }
    ::ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::wstring GitPane::remoteBrowserUrl() const
{
    return browserUrl(status_.remoteUrl);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void GitPane::drawStatusStrip(RenderContext& rc)
{
    const auto& t = rc.theme;
    int row = kStatusRow;
    int feat = kNavW;
    rc.screen.fillLine(rc.bounds.y + row, rc.bounds.x + feat, rc.bounds.w - feat,
                       L' ', t.color(render::Role::StatusBar), t.color(render::Role::StatusBar));

    std::wstring left;
    if (isRepo_)
    {
        left += status_.branch;
        if (!status_.remoteName.empty())
        {
            left += L"  \u2016  " + status_.remoteName;
        }
        if (status_.ahead > 0 || status_.behind > 0)
        {
            left += L"  ";
            if (status_.ahead > 0)
            {
                left += L"\u2191" + std::to_wstring(status_.ahead);
            }
            if (status_.behind > 0)
            {
                left += L"\u2193" + std::to_wstring(status_.behind);
            }
        }
    }
    else
    {
        left = tr(L"Not a git repository");
    }

    std::wstring right;
    if (isRepo_)
    {
        if (status_.staged > 0)
        {
            right += L" S:" + std::to_wstring(status_.staged);
        }
        if (status_.modified + status_.deleted > 0)
        {
            right += L" M:" + std::to_wstring(status_.modified + status_.deleted);
        }
        if (status_.untracked > 0)
        {
            right += L" U:" + std::to_wstring(status_.untracked);
        }
        if (op_ && !op_->finished)
        {
            wchar_t spin = L"|/-\\"[(::GetTickCount64() / 120) % 4];
            right = std::wstring(L"  ") + spin + L" " + op_->label + right;
        }
    }

    const render::Color bg = t.color(render::Role::StatusBar);
    rc.screen.putText(rc.bounds.y + row, rc.bounds.x + feat, clipRight(left, rc.bounds.w - feat - (int)right.size() - 2),
                      t.color(render::Role::StatusBarText), bg);
    int rightX = rc.bounds.x + rc.bounds.w - (int)right.size() - 1;
    if (rightX > rc.bounds.x + feat)
    {
        rc.screen.putText(rc.bounds.y + row, rightX, right,
                          isRepo_ ? t.color(render::Role::Accent) : t.color(render::Role::StatusBarText), bg);
    }
}

void GitPane::drawNav(RenderContext& rc)
{
    const auto& t = rc.theme;
    const std::wstring items[7] = {
        L"Overview", L"Changes", L"Branches", L"History",
        L"Graph", L"Remotes", L"GitHub"
    };
    rc.screen.fillLine(rc.bounds.y + kNavRow, rc.bounds.x, rc.bounds.w, L' ',
                       t.color(render::Role::Sidebar), t.color(render::Role::Sidebar));
    int x = 1;
    for (int i = 0; i < 7; ++i)
    {
        std::wstring label = L" " + items[i] + L" ";
        int w = (int)label.size();
        if (x + w >= rc.bounds.w)
        {
            break;
        }
        bool active = (i == (int)section_);
        const render::Color fg = active ? t.color(render::Role::SidebarActive)
                                        : t.color(render::Role::SidebarText);
        const render::Color bg = active ? t.color(render::Role::Accent)
                                        : t.color(render::Role::Sidebar);
        rc.screen.fillLine(rc.bounds.y + kNavRow, rc.bounds.x + x, w, L' ', fg, bg);
        rc.screen.putText(rc.bounds.y + kNavRow, rc.bounds.x + x, items[i], fg, bg, active);
        hitBoxes_.push_back({ kNavRow, x, w, L"nav", i });
        x += w + 1;
    }
}

void GitPane::drawButtons(RenderContext& rc, int row,
                          const std::vector<std::wstring>& labels, int& usedRows)
{
    int x = 1;
    int y = row;
    for (size_t i = 0; i < labels.size(); ++i)
    {
        std::wstring label = L"[" + labels[i] + L"] ";
        int w = (int)label.size();
        if (x + w >= rc.bounds.w - 1)
        {
            y += 1;
            x = 1;
            if (y >= rc.bounds.h)
            {
                break;
            }
        }
        draw::text(rc, y, x, labels[i], render::Role::Accent, render::Role::Background, true);
        rc.screen.put(rc.bounds.y + y, rc.bounds.x + x - 1, L'[',
                      rc.theme.color(render::Role::Border), rc.theme.color(render::Role::Background));
        rc.screen.put(rc.bounds.y + y, rc.bounds.x + x + (int)labels[i].size(), L']',
                      rc.theme.color(render::Role::Border), rc.theme.color(render::Role::Background));
        hitBoxes_.push_back({ y, x - 1, w, L"btn", (int)i });
        x += w;
    }
    usedRows = y + 1;
}

void GitPane::drawDull(RenderContext& rc, int row0, int col0, int w, int h)
{
    const auto& t = rc.theme;
    rc.screen.fillRect(rc.bounds.y + row0, rc.bounds.x + col0, h, w, L' ',
                       t.color(render::Role::Background), t.color(render::Role::Background));
}

void GitPane::drawOverview(RenderContext& rc)
{
    int row = kContentRow;
    if (!isRepo_)
    {
        draw::text(rc, row, kNavW, tr(L"Not a git repository"), render::Role::Muted);
        return;
    }

    auto kv = [&](const std::wstring& k, const std::wstring& v, render::Role fg = render::Role::Foreground) {
        draw::text(rc, row, kNavW, padR(k, 16), render::Role::Muted);
        draw::text(rc, row, kNavW + 17, clipRight(v, rc.bounds.w - kNavW - 18), fg);
        ++row;
    };

    kv(L"Repository", fs::path(status_.repoPath).filename().wstring(), render::Role::Accent);
    kv(L"Path", status_.repoPath);
    kv(L"Branch", status_.branch, status_.detachedHead ? render::Role::Warning : render::Role::Accent);
    if (status_.ahead > 0)
    {
        kv(L"Ahead", std::to_wstring(status_.ahead) + L" commit(s) to push", render::Role::Success);
    }
    if (status_.behind > 0)
    {
        kv(L"Behind", std::to_wstring(status_.behind) + L" commit(s) to pull", render::Role::Warning);
    }
    if (!status_.remoteName.empty())
    {
        kv(L"Remote", status_.remoteName + L"  \u27a4  " + status_.remoteUrl);
    }
    kv(L"Changes", (std::wstring(L"staged ") + std::to_wstring(status_.staged) +
                    L"  ·  modified " + std::to_wstring(status_.modified) +
                    L"  ·  untracked " + std::to_wstring(status_.untracked) +
                    L"  ·  deleted " + std::to_wstring(status_.deleted)),
       status_.staged > 0 ? render::Role::Success : render::Role::Foreground);
    ++row;
    draw::text(rc, row++, kNavW, tr(L"Last commit"), render::Role::PanelTitle);
    kv(L"", status_.lastCommitSubject.empty() ? L"(no commits yet)" : status_.lastCommitSubject, render::Role::Accent);
    if (!status_.lastCommitShort.empty())
    {
        kv(L"Hash", status_.lastCommitShort, render::Role::Muted);
        kv(L"Author", status_.lastCommitAuthor);
        kv(L"Date", status_.lastCommitDate, render::Role::Muted);
    }
    ++row;
    int used = 0;
    const std::vector<std::wstring> buttons =
        status_.remoteUrl.empty()
            ? std::vector<std::wstring>{ L"Refresh", L"Fetch" }
            : std::vector<std::wstring>{ L"Open on GitHub", L"Refresh", L"Fetch", L"Push", L"Pull" };
    drawButtons(rc, row, buttons, used);
    draw::text(rc, rc.bounds.h - 1, kNavW,
               L"[O] open   [F] fetch   [P] push   [L] pull   [S] stash   [X] pop   [C] clone   [F5] refresh",
               render::Role::Muted);
}

void GitPane::drawChanges(RenderContext& rc)
{
    const auto& t = rc.theme;
    if (!isRepo_)
    {
        draw::text(rc, kContentRow, kNavW, tr(L"Not a git repository"), render::Role::Muted);
        return;
    }
    if (focus_ == Focus::None)
    {
        focus_ = Focus::Files;
    }

    int fileW = std::min(std::max(34, rc.bounds.w * 3 / 10), 52);
    int labelRow = kContentRow;
    int listTop = kContentRow + 1;
    int listBot = rc.bounds.h - 6; // reserve commit fields + buttons + hints
    int listH = listBot - listTop;
    if (listH < 1)
    {
        listH = 1;
    }

    // File list.
    draw::text(rc, labelRow, kNavW, L"Files", focus_ == Focus::Files ? render::Role::PanelTitle : render::Role::Muted);
    if (files_.empty())
    {
        draw::text(rc, listTop, kNavW, L"No changes detected", render::Role::Muted);
    }
    ensureVisible(fileScroll_, selectedFile_, listH);
    for (int i = 0; i < listH; ++i)
    {
        int idx = fileScroll_ + i;
        if (idx >= (int)files_.size())
        {
            break;
        }
        const auto& f = files_[idx];
        bool sel = (idx == selectedFile_ && focus_ == Focus::Files);
        const render::Color bg = sel ? t.color(render::Role::Selection) : t.color(render::Role::Background);
        int fy = listTop + i;
        rc.screen.fillLine(rc.bounds.y + fy, rc.bounds.x + kNavW, rc.bounds.w - kNavW, L' ', t.color(render::Role::Foreground), bg);
        std::wstring mark = statusMark(f.indexStatus, f.workTreeStatus);
        rc.screen.putText(rc.bounds.y + fy, rc.bounds.x + kNavW,
                          mark, t.color(fileRole(f)), bg, sel);
        std::wstring name = f.path;
        if (f.untracked) name = name + L"  (untracked)";
        rc.screen.putText(rc.bounds.y + fy, rc.bounds.x + kNavW + 4,
                          clipRight(name, fileW - 5), t.color(fileRole(f)), bg, false);
        rc.screen.putText(rc.bounds.y + fy, rc.bounds.x + kNavW + fileW - 8,
                          clipRight(f.label, 7), t.color(render::Role::Muted), bg, false);
        hitBoxes_.push_back({ fy, kNavW, rc.bounds.w - kNavW, L"file", idx });
    }

    // Diff area.
    int diffCol = kNavW + fileW;
    int diffW = rc.bounds.w - diffCol;
    draw::text(rc, labelRow, diffCol, L"Diff  (" + std::wstring(diffIsCached_ ? L"staged" : L"working tree") + L")",
               render::Role::Muted);
    if (diffLoading_)
    {
        draw::text(rc, listTop, diffCol, L"Loading diff...", render::Role::Muted);
    }
    else if (!diff_.empty())
    {
        drawDiff(rc, listTop, diffCol, diffW, listH, diff_, listH);
    }
    else if (!files_.empty() && files_[selectedFile_].untracked)
    {
        draw::text(rc, listTop, diffCol, L"Untracked file - not yet in version control.",
                   render::Role::Muted);
        draw::text(rc, listTop + 1, diffCol, L"Press [S]/[Space] or Stage to add it.",
                   render::Role::Muted);
    }
    else
    {
        draw::text(rc, listTop, diffCol, L"No diff for this file.", render::Role::Muted);
    }

    // Commit fields.
    int msgRow = rc.bounds.h - 5;
    int descRow = rc.bounds.h - 4;
    auto drawField = [&](int frow, const std::wstring& label, const std::wstring& value,
                         int cursor, bool active) {
        const render::Color bg = active ? t.color(render::Role::Selection)
                                        : t.color(render::Role::CommandBar);
        const render::Color fg = t.color(render::Role::CommandBarText);
        rc.screen.fillLine(rc.bounds.y + frow, rc.bounds.x + kNavW,
                           rc.bounds.w - kNavW, L' ', fg, bg);
        rc.screen.putText(rc.bounds.y + frow, rc.bounds.x + kNavW, label,
                          t.color(render::Role::Accent), bg);
        int cx = kNavW + (int)label.size();
        rc.screen.putText(rc.bounds.y + frow, rc.bounds.x + cx,
                          clipRight(value, rc.bounds.w - kNavW - (int)label.size() - 1),
                          fg, bg);
        if (active)
        {
            int valueW = std::min((int)value.size(), rc.bounds.w - kNavW - (int)label.size() - 1);
            rc.screen.put(rc.bounds.y + frow, rc.bounds.x + cx + valueW, L'\u258e',
                          t.color(render::Role::Accent), bg);
        }
        hitBoxes_.push_back({ frow, kNavW, rc.bounds.w - kNavW, L"cmf", (frow == msgRow) ? 0 : 1 });
    };
    bool msgActive = focus_ == Focus::CommitMsg;
    bool descActive = focus_ == Focus::CommitDesc;
    drawField(msgRow, L"Message:  ", commitMsg_, msgActive ? commitCursor_ : -1, msgActive);
    drawField(descRow, L"Desc:     ", commitDesc_, descActive ? commitCursor_ : -1, descActive);

    // Action + commit buttons.
    int used = 0;
    drawButtons(rc, rc.bounds.h - 3,
                { L"Stage", L"Unstage", L"Discard", L"Stage all", L"Unstage all", L"Split" },
                used);
    drawButtons(rc, rc.bounds.h - 2, { L"Commit", L"Commit & Push" }, used);

    std::wstring hint;
    if (focus_ == Focus::CommitMsg || focus_ == Focus::CommitDesc)
    {
        hint = L"Type a message   [Enter] commit   [Tab] next field";
    }
    else
    {
        hint = L"[S] stage   [U] unstage   [D] discard   [A] stage all   [Z] unstage all   [X] split diff   [C] commit   [P] commit & push   [Tab] fields";
    }
    draw::text(rc, rc.bounds.h - 1, kNavW, clipRight(hint, rc.bounds.w - kNavW - 1), render::Role::Muted);
}

// Generic diff renderer (unified or side-by-side) inside the given box.
void GitPane::drawDiff(RenderContext& rc, int row0, int col0, int w, int h,
                       const std::vector<git::GitDiffLine>& diff, int& usedRows)
{
    const auto& t = rc.theme;

    auto drawLineNo = [&](int row, int x, int no, int width) {
        std::wstring s = no < 0 ? std::wstring(width, L' ') : std::wstring(width, L' ');
        if (no >= 0)
        {
            s = padL(std::to_wstring(no), width);
        }
        rc.screen.putText(rc.bounds.y + row, rc.bounds.x + x, s,
                          t.color(render::Role::Muted),
                          t.color(render::Role::Background));
    };

    if (!sideBySide_ || w < 40)
    {
        // Unified view.
        const int numW = 4;
        int rows = 0;
        for (int i = diffScroll_; i < (int)diff.size(); ++i)
        {
            if (rows >= h)
            {
                break;
            }
            const auto& dl = diff[i];
            int y = row0 + rows;
            render::Role fg = render::Role::Foreground;
            bool bold = false;
            if (dl.type == L'@')
            {
                fg = render::Role::Accent;
                bold = true;
            }
            else if (dl.type == L'h')
            {
                fg = render::Role::Muted;
            }
            else if (dl.type == L'+')
            {
                fg = render::Role::Success;
            }
            else if (dl.type == L'-')
            {
                fg = render::Role::Error;
            }
            rc.screen.fillLine(rc.bounds.y + y, rc.bounds.x + col0, w, L' ',
                               t.color(fg), t.color(render::Role::Background));
            if (dl.type == L'@' || dl.type == L'h')
            {
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + col0,
                                  clipRight(dl.text, (size_t)w), t.color(fg),
                                  t.color(render::Role::Background), bold);
            }
            else
            {
                drawLineNo(y, col0 + 0, dl.oldNo, numW);
                drawLineNo(y, col0 + numW, dl.newNo, numW);
                int x = col0 + numW * 2;
                rc.screen.put(rc.bounds.y + y, rc.bounds.x + x, dl.type, t.color(fg),
                              t.color(render::Role::Background), bold);
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + x + 1,
                                  clipRight(dl.text, (size_t)(w - numW * 2 - 2)),
                                  t.color(fg), t.color(render::Role::Background), bold);
            }
            ++rows;
        }
        usedRows = rows;
        return;
    }

    // Side-by-side view: pair removed/added lines per hunk.
    struct PairRow
    {
        int oldNo = -1, newNo = -1;
        std::wstring left, right;
        wchar_t kind = L'c'; // 'c' context, 'r' removed/added pair, 'h' header, '@' hunk
        std::wstring header;
    };
    std::vector<PairRow> pairs;
    std::vector<std::pair<int, std::wstring>> dels;
    std::vector<std::pair<int, std::wstring>> adds;
    auto flush = [&]() {
        size_t n = std::max(dels.size(), adds.size());
        for (size_t k = 0; k < n; ++k)
        {
            PairRow r;
            r.kind = L'r';
            if (k < dels.size())
            {
                r.oldNo = dels[k].first;
                r.left = dels[k].second;
            }
            if (k < adds.size())
            {
                r.newNo = adds[k].first;
                r.right = adds[k].second;
            }
            pairs.push_back(std::move(r));
        }
        dels.clear();
        adds.clear();
    };
    for (const auto& dl : diff)
    {
        if (dl.type == L'@')
        {
            flush();
            PairRow r;
            r.kind = L'@';
            r.header = dl.text;
            pairs.push_back(std::move(r));
        }
        else if (dl.type == L'h')
        {
            flush();
            PairRow r;
            r.kind = L'h';
            r.header = dl.text;
            pairs.push_back(std::move(r));
        }
        else if (dl.type == L' ')
        {
            flush();
            PairRow r;
            r.kind = L'c';
            r.oldNo = dl.oldNo;
            r.newNo = dl.newNo;
            r.left = dl.text;
            r.right = dl.text;
            pairs.push_back(std::move(r));
        }
        else if (dl.type == L'-')
        {
            dels.emplace_back(dl.oldNo, dl.text);
        }
        else if (dl.type == L'+')
        {
            adds.emplace_back(dl.newNo, dl.text);
        }
    }
    flush();

    int half = w / 2;
    int rows = 0;
    for (int i = diffScroll_; i < (int)pairs.size(); ++i)
    {
        if (rows >= h)
        {
            break;
        }
        const auto& p = pairs[i];
        int y = row0 + rows;
        if (p.kind == L'@' || p.kind == L'h')
        {
            rc.screen.fillLine(rc.bounds.y + y, rc.bounds.x + col0, w, L' ',
                               t.color(render::Role::Accent), t.color(render::Role::Background));
            rc.screen.putText(rc.bounds.y + y, rc.bounds.x + col0,
                              clipRight(p.header, (size_t)w), t.color(render::Role::Accent),
                              t.color(render::Role::Background), true);
        }
        else
        {
            render::Role lfg = p.kind == L'c' ? render::Role::Foreground : render::Role::Error;
            render::Role rfg = p.kind == L'c' ? render::Role::Foreground : render::Role::Success;
            rc.screen.fillLine(rc.bounds.y + y, rc.bounds.x + col0, half, L' ',
                               t.color(render::Role::Foreground), t.color(render::Role::Background));
            rc.screen.fillLine(rc.bounds.y + y, rc.bounds.x + col0 + half, half, L' ',
                               t.color(render::Role::Foreground), t.color(render::Role::Background));
            int lx = col0;
            int rx = col0 + half;
            if (p.kind == L'c')
            {
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + lx, padL(std::to_wstring(p.oldNo), 4) + L" ",
                                  t.color(render::Role::Muted), t.color(render::Role::Background));
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + lx + 5, clipRight(p.left, (size_t)(half - 5)),
                                  t.color(lfg), t.color(render::Role::Background));
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + rx, padL(std::to_wstring(p.newNo), 4) + L" ",
                                  t.color(render::Role::Muted), t.color(render::Role::Background));
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + rx + 5, clipRight(p.right, (size_t)(half - 5)),
                                  t.color(rfg), t.color(render::Role::Background));
            }
            else
            {
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + lx, padL(std::to_wstring(p.oldNo), 4) + L" ",
                                  t.color(render::Role::Muted), t.color(render::Role::Background));
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + rx, padL(std::to_wstring(p.newNo), 4) + L" ",
                                  t.color(render::Role::Muted), t.color(render::Role::Background));
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + lx + 5, clipRight(p.left, (size_t)(half - 5)),
                                  t.color(lfg), t.color(render::Role::Background));
                rc.screen.putText(rc.bounds.y + y, rc.bounds.x + rx + 5, clipRight(p.right, (size_t)(half - 5)),
                                  t.color(rfg), t.color(render::Role::Background));
            }
        }
        ++rows;
    }
    usedRows = rows;
}

void GitPane::drawBranches(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::text(rc, kContentRow, kNavW, L"Branches", render::Role::PanelTitle);
    int listH = std::max(1, rc.bounds.h - 7);
    ensureVisible(branchScroll_, selectedBranch_, listH);
    for (int i = 0; i < listH; ++i)
    {
        int idx = branchScroll_ + i;
        if (idx >= (int)branches_.size())
        {
            break;
        }
        const auto& b = branches_[idx];
        bool sel = (idx == selectedBranch_);
        const render::Color bg = sel ? t.color(render::Role::Selection) : t.color(render::Role::Background);
        int y = kContentRow + 1 + i;
        rc.screen.fillLine(rc.bounds.y + y, rc.bounds.x + kNavW, rc.bounds.w - kNavW, L' ',
                           t.color(render::Role::Foreground), bg);
        std::wstring marker = b.isCurrent ? L"\u25b8" : L" ";
        rc.screen.put(rc.bounds.y + y, rc.bounds.x + kNavW, marker[0],
                      t.color(b.isCurrent ? render::Role::Success : render::Role::Muted), bg);
        rc.screen.putText(rc.bounds.y + y, rc.bounds.x + kNavW + 1,
                          clipRight(b.name, 30), t.color(b.isCurrent ? render::Role::Success : render::Role::Foreground), bg, b.isCurrent);
        int cx = kNavW + 32;
        if (!b.isRemote)
        {
            std::wstring ab;
            if (b.ahead > 0) ab += L"\u2191" + std::to_wstring(b.ahead);
            if (b.behind > 0) ab += L"\u2193" + std::to_wstring(b.behind);
            if (!b.upstream.empty())
            {
                ab += (ab.empty() ? L"" : L" ") + b.upstream;
            }
            rc.screen.putText(rc.bounds.y + y, rc.bounds.x + cx, clipRight(ab, 26),
                              t.color(render::Role::Muted), bg);
        }
        else
        {
            rc.screen.putText(rc.bounds.y + y, rc.bounds.x + cx, L"(remote)",
                              t.color(render::Role::Muted), bg);
        }
        rc.screen.putText(rc.bounds.y + y, rc.bounds.x + rc.bounds.w - 12,
                          b.lastCommit, t.color(render::Role::Muted), bg);
        hitBoxes_.push_back({ y, kNavW, rc.bounds.w - kNavW, L"branch", idx });
    }
    int used = 0;
    drawButtons(rc, rc.bounds.h - 3,
                { L"New", L"Checkout", L"Rename", L"Delete", L"Merge", L"Fetch" }, used);
    draw::text(rc, rc.bounds.h - 1, kNavW,
               L"[N] new   [Enter] checkout   [R] rename   [D] delete   [M] merge   [F] fetch",
               render::Role::Muted);
}

void GitPane::drawHistory(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::text(rc, kContentRow, kNavW, L"Commits", render::Role::PanelTitle);
    int listTop = kContentRow + 1;
    int listH = std::max(1, std::min((int)commits_.size(), rc.bounds.h - 7));
    ensureVisible(historyScroll_, selectedCommit_, listH);
    for (int i = 0; i < listH; ++i)
    {
        int idx = historyScroll_ + i;
        if (idx >= (int)commits_.size())
        {
            break;
        }
        const auto& c = commits_[idx];
        bool sel = (idx == selectedCommit_);
        const render::Color bg = sel ? t.color(render::Role::Selection) : t.color(render::Role::Background);
        int y = listTop + i;
        rc.screen.fillLine(rc.bounds.y + y, rc.bounds.x + kNavW, rc.bounds.w - kNavW, L' ',
                           t.color(render::Role::Foreground), bg);
        rc.screen.putText(rc.bounds.y + y, rc.bounds.x + kNavW, clipRight(c.shortHash, 9),
                          t.color(render::Role::Accent), bg, sel);
        rc.screen.putText(rc.bounds.y + y, rc.bounds.x + kNavW + 10,
                          clipRight(c.subject, (size_t)(rc.bounds.w - kNavW - 42)),
                          t.color(render::Role::Foreground), bg);
        rc.screen.putText(rc.bounds.y + y, rc.bounds.x + rc.bounds.w - 30,
                          clipRight(c.author, 18), t.color(render::Role::Muted), bg);
        rc.screen.putText(rc.bounds.y + y, rc.bounds.x + rc.bounds.w - 11,
                          clipRight(c.relativeDate, 10), t.color(render::Role::Muted), bg);
        if (c.isHead)
        {
            rc.screen.put(rc.bounds.y + y, rc.bounds.x + rc.bounds.w - 12, L'*',
                          t.color(render::Role::Success), bg);
        }
        hitBoxes_.push_back({ y, kNavW, rc.bounds.w - kNavW, L"commit", idx });
    }

    // Commit detail pane.
    int detailTop = listTop + listH + 1;
    int detailRows = rc.bounds.h - detailTop - 3;
    if (detailRows > 0)
    {
        int dy = detailTop;
        draw::text(rc, dy, kNavW, L"Commit detail", render::Role::PanelTitle);
        if (commitDetailLoading_)
        {
            draw::text(rc, dy + 1, kNavW, L"Loading...", render::Role::Muted);
        }
        else if (!commits_.empty() && commitDetail_.empty())
        {
            draw::text(rc, dy + 1, kNavW, L"No detail loaded.", render::Role::Muted);
        }
        else
        {
            std::wstring stat = clipRight(commitDetail_, 120);
            draw::text(rc, dy + 1, kNavW, stat, render::Role::Success);
        }
    }
    int used = 0;
    drawButtons(rc, rc.bounds.h - 3,
                { L"Copy hash", L"Create branch", L"Revert", L"Reset" }, used);
    draw::text(rc, rc.bounds.h - 1, kNavW,
               L"[C] copy hash   [B] branch here   [V] revert   [R] reset   [F5] refresh log",
               render::Role::Muted);
}

void GitPane::drawGraph(RenderContext& rc)
{
    draw::text(rc, kContentRow, kNavW, L"Commit graph", render::Role::PanelTitle);
    int listTop = kContentRow + 1;
    int listH = std::max(1, rc.bounds.h - 7);
    std::vector<std::wstring> lines;
    {
        size_t pos = 0;
        while (pos <= graph_.size() && lines.size() < 600)
        {
            size_t nl = graph_.find(L'\n', pos);
            if (nl == std::wstring::npos)
            {
                nl = graph_.size();
            }
            std::wstring line = graph_.substr(pos, nl - pos);
            if (!line.empty() && line.back() == L'\r')
            {
                line.pop_back();
            }
            lines.push_back(line);
            pos = nl + 1;
        }
    }
    for (int i = 0; i < listH; ++i)
    {
        int idx = graphScroll_ + i;
        if (idx >= (int)lines.size())
        {
            break;
        }
        std::wstring ln = lines[idx];
        render::Role fg = render::Role::Foreground;
        if (ln.find(L"*") != std::wstring::npos)
        {
            fg = render::Role::Accent;
        }
        else if (ln.find(L"|") != std::wstring::npos)
        {
            fg = render::Role::Muted;
        }
        draw::text(rc, listTop + i, kNavW, clipRight(ln, rc.bounds.w - kNavW - 1), fg);
    }
    int used = 0;
    drawButtons(rc, rc.bounds.h - 3, { L"Refresh" }, used);
    draw::text(rc, rc.bounds.h - 1, kNavW, L"[F5]/[R] refresh graph",
               render::Role::Muted);
}

void GitPane::drawRemotes(RenderContext& rc)
{
    const auto& t = rc.theme;
    draw::text(rc, kContentRow, kNavW, L"Remotes", render::Role::PanelTitle);
    int listTop = kContentRow + 1;
    int listH = std::max(1, rc.bounds.h - 7);
    ensureVisible(remoteScroll_, selectedRemote_, listH);
    for (int i = 0; i < listH; ++i)
    {
        int idx = remoteScroll_ + i;
        if (idx >= (int)remotes_.size())
        {
            break;
        }
        const auto& r = remotes_[idx];
        bool sel = (idx == selectedRemote_);
        const render::Color bg = sel ? t.color(render::Role::Selection) : t.color(render::Role::Background);
        int y = listTop + i;
        rc.screen.fillLine(rc.bounds.y + y, rc.bounds.x + kNavW, rc.bounds.w - kNavW, L' ',
                           t.color(render::Role::Foreground), bg);
        rc.screen.putText(rc.bounds.y + y, rc.bounds.x + kNavW, clipRight(r.name, 16),
                          t.color(render::Role::Accent), bg);
        rc.screen.putText(rc.bounds.y + y, rc.bounds.x + kNavW + 18,
                          clipRight(r.url, rc.bounds.w - kNavW - 19), t.color(render::Role::Foreground), bg);
        hitBoxes_.push_back({ y, kNavW, rc.bounds.w - kNavW, L"remote", idx });
    }
    if (remotes_.empty())
    {
        draw::text(rc, listTop, kNavW, L"No remotes configured.",
                   render::Role::Muted);
    }
    int used = 0;
    drawButtons(rc, rc.bounds.h - 3,
                { L"Fetch", L"Push", L"Pull", L"Add", L"Remove", L"Open" }, used);
    draw::text(rc, rc.bounds.h - 1, kNavW,
               L"[F] fetch   [P] push   [L] pull   [A] add   [D] remove   [O] open in browser",
               render::Role::Muted);
}

void GitPane::drawGitHub(RenderContext& rc)
{
    const auto& t = rc.theme;
    int y = kContentRow;
    if (!ghConnected_)
    {
        draw::text(rc, y, kNavW, L"Not connected to GitHub.", render::Role::Muted);
        draw::text(rc, y + 1, kNavW, L"Create a token at  github.com/settings/tokens",
                   render::Role::Muted);
        draw::text(rc, y + 2, kNavW, L"then press [C] or the Connect button.",
                   render::Role::Muted);
        int used = 0;
        drawButtons(rc, y + 4, { L"Connect" }, used);
        draw::text(rc, rc.bounds.h - 1, kNavW, L"[C] connect with a personal access token",
                   render::Role::Muted);
        return;
    }

    if (ghLoading_ && ghRepos_.empty())
    {
        draw::text(rc, y, kNavW, L"Loading repositories...", render::Role::Muted);
    }

    std::wstring acc = ghAccountInfo_.empty() ? L"@" + ghUser_.login : ghAccountInfo_;
    draw::text(rc, y, kNavW, L"Account:", render::Role::Muted);
    draw::text(rc, y, kNavW + 9, clipRight(acc, rc.bounds.w - kNavW - 30),
               render::Role::Accent, render::Role::Background, true);

    int listTop = y + 2;
    int halfH = std::max(1, (rc.bounds.h - listTop - 5) / 2);
    int prTop = listTop + halfH + 1;

    // Repositories.
    ensureVisible(ghScroll_, ghRepoIdx_, halfH);
    for (int i = 0; i < halfH; ++i)
    {
        int idx = ghScroll_ + i;
        if (idx >= (int)ghRepos_.size())
        {
            break;
        }
        const auto& r = ghRepos_[idx];
        bool sel = (idx == ghRepoIdx_);
        const render::Color bg = sel ? t.color(render::Role::Selection) : t.color(render::Role::Background);
        int ry = listTop + i;
        rc.screen.fillLine(rc.bounds.y + ry, rc.bounds.x + kNavW, rc.bounds.w - kNavW, L' ',
                           t.color(render::Role::Foreground), bg);
        rc.screen.putText(rc.bounds.y + ry, rc.bounds.x + kNavW,
                          clipRight(r.fullName, 34), t.color(sel ? render::Role::Accent : render::Role::Foreground), bg);
        std::wstring meta = r.isPrivate ? L"private" : L"public";
        meta += L"  \u2605" + std::to_wstring(r.stars);
        rc.screen.putText(rc.bounds.y + ry, rc.bounds.x + kNavW + 36, clipRight(meta, 14),
                          t.color(render::Role::Muted), bg);
        rc.screen.putText(rc.bounds.y + ry, rc.bounds.x + rc.bounds.w - 18,
                          clipRight(r.defaultBranch, 16), t.color(render::Role::Muted), bg);
        hitBoxes_.push_back({ ry, kNavW, rc.bounds.w - kNavW, L"ghrepo", idx });
    }
    if (ghRepos_.empty() && !ghLoading_)
    {
        draw::text(rc, listTop, kNavW, L"No repositories.", render::Role::Muted);
    }

    // Pull requests + issues for the selected repository.
    draw::text(rc, prTop, kNavW, clipRight(L"Pull requests  " + (ghRepoSelected_.empty() ? L"(select a repo)" : L"\u2014 " + ghRepoSelected_), 55),
               render::Role::PanelTitle);
    for (int i = 0; i < 4; ++i)
    {
        if (i >= (int)ghPRs_.size())
        {
            break;
        }
        const auto& pr = ghPRs_[i];
        std::wstring line = L"#" + pr.number + L"  " + pr.title;
        draw::text(rc, prTop + 1 + i, kNavW + 2, clipRight(line, 42), render::Role::Foreground);
        draw::text(rc, prTop + 1 + i, rc.bounds.x + 60 - rc.bounds.x,
                   clipRight(pr.author, 10), render::Role::Muted);
    }
    if (ghPRs_.empty() && !ghRepoSelected_.empty())
    {
        draw::text(rc, prTop + 1, kNavW + 2, L"(no open pull requests)",
                   render::Role::Muted);
    }

    int issueTop = prTop + halfH;
    if (issueTop + 5 < rc.bounds.h)
    {
        draw::text(rc, issueTop, kNavW, L"Issues  " +
                   (ghRepoSelected_.empty() ? L"" : L"\u2014 " + ghRepoSelected_),
                   render::Role::PanelTitle);
        for (int i = 0; i < 4; ++i)
        {
            if (i >= (int)ghIssues_.size())
            {
                break;
            }
            std::wstring line = L"#" + ghIssues_[i].number + L"  " + ghIssues_[i].title;
            draw::text(rc, issueTop + 1 + i, kNavW + 2, clipRight(line, 42),
                       render::Role::Foreground);
        }
        if (ghIssues_.empty() && !ghRepoSelected_.empty())
        {
            draw::text(rc, issueTop + 1, kNavW + 2, L"(no open issues)",
                       render::Role::Muted);
        }
    }

    int used = 0;
    drawButtons(rc, rc.bounds.h - 3,
                { L"Open", L"New PR", L"New Issue", L"Refresh", L"Disconnect" }, used);
    draw::text(rc, rc.bounds.h - 1, kNavW,
               L"[O] open on GitHub   [N] new PR   [I] new issue   [R] refresh   [C] connect/change token",
               render::Role::Muted);
}

void GitPane::drawDialog(RenderContext& rc)
{
    const auto& t = rc.theme;
    int fieldCount = dialogFieldCount_;
    int optionLines = (int)dialogOptions_.size();
    if (optionLines > 8)
    {
        optionLines = 8;
    }
    int bw = std::min(76, rc.bounds.w - 10);
    int bh = 4 + fieldCount + (dialog_ == DialogKind::MergeBranch ? optionLines + 1 : 2);
    if (bh > rc.bounds.h - 4)
    {
        bh = rc.bounds.h - 4;
    }
    int bx = std::max(1, (rc.bounds.w - bw) / 2);
    int by = std::max(1, (rc.bounds.h - bh) / 2);

    const render::Color border = t.color(render::Role::Border);
    const render::Color bg = t.color(render::Role::Sidebar);
    rc.screen.drawBorder(rc.bounds.y + by, rc.bounds.x + bx, bh, bw, border, bg);
    rc.screen.fillRect(rc.bounds.y + by, rc.bounds.x + bx, bh, bw, L' ',
                       t.color(render::Role::SidebarText), bg);

    size_t titleMax = (size_t)(bw - 4);
    std::wstring title = dialogTitle_.empty() ? L"KShell" : dialogTitle_;
    rc.screen.putText(rc.bounds.y + by, rc.bounds.x + bx + 1, clipRight(title, titleMax),
                      t.color(render::Role::Accent), bg, true);

    int row = by + 1;
    if (dialog_ == DialogKind::MergeBranch)
    {
        ensureVisible(dialogOptionScroll_, dialogOptionIdx_, optionLines);
        std::wstring opts = L"Select branch:";
        rc.screen.putText(rc.bounds.y + row + 1, rc.bounds.x + bx + 1, opts,
                          t.color(render::Role::Muted), bg);
        for (int i = 0; i < optionLines; ++i)
        {
            int idx = dialogOptionScroll_ + i;
            if (idx >= (int)dialogOptions_.size())
            {
                break;
            }
            bool sel = (idx == dialogOptionIdx_);
            std::wstring label = (sel ? L"\u25b8 " : L"  ") + dialogOptions_[idx];
            rc.screen.fillLine(rc.bounds.y + row + 2 + i, rc.bounds.x + bx + 1, bw - 2, L' ',
                               t.color(render::Role::Foreground),
                               sel ? t.color(render::Role::Selection) : bg);
            rc.screen.putText(rc.bounds.y + row + 2 + i, rc.bounds.x + bx + 1,
                              clipRight(label, (size_t)(bw - 4)),
                              t.color(sel ? render::Role::Accent : render::Role::Foreground),
                              sel ? t.color(render::Role::Selection) : bg, sel);
            hitBoxes_.push_back({ row + 2 + i, bx + 1, bw - 2, L"dopt", idx });
        }
        row += 2 + optionLines;
    }
    else
    {
        for (int i = 0; i < fieldCount; ++i)
        {
            bool active = (dialogFieldFocus_ == i);
            std::wstring label = dialogFieldTitles_[i] + L":";
            rc.screen.putText(rc.bounds.y + row + 1, rc.bounds.x + bx + 1, label,
                              t.color(active ? render::Role::Accent : render::Role::Muted), bg);
            int vx = bx + 1 + (int)label.size();
            rc.screen.putText(rc.bounds.y + row + 1, rc.bounds.x + vx,
                              clipRight(dialogFields_[i], (size_t)(bw - (int)label.size() - 3)),
                              t.color(render::Role::Foreground), bg);
            if (active)
            {
                int cur = std::min(dialogFieldCursor_, (int)dialogFields_[i].size());
                int cx = vx + cur;
                if (cx < bx + bw - 1)
                {
                    rc.screen.put(rc.bounds.y + row + 1, rc.bounds.x + cx, L'\u258e',
                                  t.color(render::Role::Accent), bg);
                }
            }
            hitBoxes_.push_back({ row + 1, bx + 1, bw - 2, L"dfield", i });
            ++row;
        }
        row += 1;
    }

    // Buttons.
    std::wstring labels[2] = { dialogButtonTitle_.empty() ? L"OK" : dialogButtonTitle_, L"Cancel" };
    int totalButtons = dialogButtons_; // 1 or 2

    // Layout: center buttons on the button row.
    int btnY = by + bh - 2;
    int totalW = 0;
    for (int i = 0; i < totalButtons; ++i)
    {
        totalW += (int)labels[i].size() + 4;
    }
    int bx2 = bx + (bw - totalW) / 2;
    for (int i = 0; i < totalButtons; ++i)
    {
        bool focused = (dialogFieldFocus_ >= fieldCount && dialogFieldFocus_ - fieldCount == i);
        rc.screen.fillLine(rc.bounds.y + btnY, rc.bounds.x + bx2, (int)labels[i].size() + 4, L' ',
                           t.color(focused ? render::Role::Accent : render::Role::SidebarText),
                           focused ? t.color(render::Role::Accent) : bg);
        rc.screen.putText(rc.bounds.y + btnY, rc.bounds.x + bx2 + 2, labels[i],
                          focused ? t.color(render::Role::AccentText) : t.color(render::Role::SidebarText),
                          focused ? t.color(render::Role::Accent) : bg, false);
        hitBoxes_.push_back({ btnY, bx2, (int)labels[i].size() + 4, L"dbtn", i });
        bx2 += (int)labels[i].size() + 4;
    }
}

void GitPane::draw(RenderContext& rc)
{
    if (op_ && op_->finished)
    {
        applyOp();
    }
    hitBoxes_.clear();
    draw::clear(rc, render::Role::Background);
    draw::header(rc, tr(L"Git"));
    drawStatusStrip(rc);
    drawNav(rc);

    switch (section_)
    {
    case Section::Overview: drawOverview(rc); break;
    case Section::Changes: drawChanges(rc); break;
    case Section::Branches: drawBranches(rc); break;
    case Section::History: drawHistory(rc); break;
    case Section::Graph: drawGraph(rc); break;
    case Section::Remotes: drawRemotes(rc); break;
    case Section::GitHub: drawGitHub(rc); break;
    }

    // Transient notice on the hints line.
    if (!notice_.empty() && (::GetTickCount64() - noticeTime_ < 6000))
    {
        draw::text(rc, rc.bounds.h - 1, kNavW, clipRight(notice_, rc.bounds.w - kNavW - 1),
                   render::Role::Warning);
    }

    if (dialog_ != DialogKind::None)
    {
        drawDialog(rc);
    }
}

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
