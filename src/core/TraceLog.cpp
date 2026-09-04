#include "core/TraceLog.h"

#include <windows.h>

namespace kshell
{

TraceLog::TraceLog() = default;

namespace
{
std::wstring nowString()
{
    SYSTEMTIME st{};
    ::GetLocalTime(&st);
    wchar_t buf[64];
    swprintf(buf, 64, L"%02d:%02d:%02d",
             (int)st.wHour, (int)st.wMinute, (int)st.wSecond);
    return std::wstring(buf);
}
} // namespace

void TraceLog::setEnabled(bool on)
{
    std::lock_guard<std::mutex> lk(mtx_);
    enabled_ = on;
}

bool TraceLog::enabled() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return enabled_;
}

void TraceLog::record(TraceKind kind, std::wstring message)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (!enabled_ && kind != TraceKind::Variable)
    {
        return;
    }
    TraceEvent e;
    e.kind = kind;
    e.message = std::move(message);
    e.timestamp = nowString();
    e.id = nextId_++;
    events_.push_back(std::move(e));
    if (events_.size() > 2000)
    {
        events_.erase(events_.begin(), events_.begin() + (events_.size() - 2000));
    }
}

void TraceLog::recordExecution(const std::wstring& command, uint32_t pid, int exitCode,
                               long long durationMs, const std::wstring& output)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (!enabled_)
    {
        return;
    }
    TraceEvent e;
    e.kind = TraceKind::Execution;
    e.message = command;
    e.timestamp = nowString();
    e.pid = pid;
    e.exitCode = exitCode;
    e.durationMs = durationMs;
    std::wstring out = output;
    if (out.size() > 200)
    {
        out = out.substr(0, 197) + L"...";
    }
    e.message = command + L"  [pid=" + std::to_wstring(pid) +
                L" exit=" + std::to_wstring(exitCode) +
                L" " + std::to_wstring(durationMs) + L"ms]";
    if (!out.empty())
    {
        e.message += L"  -> " + out;
    }
    e.id = nextId_++;
    events_.push_back(std::move(e));
    if (events_.size() > 2000)
    {
        events_.erase(events_.begin(), events_.begin() + (events_.size() - 2000));
    }
}

void TraceLog::clear()
{
    std::lock_guard<std::mutex> lk(mtx_);
    events_.clear();
}

std::vector<TraceEvent> TraceLog::events() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return events_;
}

std::vector<TraceEvent> TraceLog::eventsOf(TraceKind kind) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<TraceEvent> result;
    for (const auto& e : events_)
    {
        if (e.kind == kind)
        {
            result.push_back(e);
        }
    }
    return result;
}

size_t TraceLog::count() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return events_.size();
}

} // namespace kshell
