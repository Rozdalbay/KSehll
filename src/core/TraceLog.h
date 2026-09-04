#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace kshell
{

enum class TraceKind
{
    CommandTrace,  // set -x style: expanded command / cd / var expansion before execution
    Execution,     // per-command: pid, exit code, duration, output
    Variable,      // variable set/unset/change
    Directory,     // cd changes
};

struct TraceEvent
{
    TraceKind kind = TraceKind::CommandTrace;
    std::wstring message;
    std::wstring timestamp;
    uint32_t pid = 0;
    int exitCode = 0;
    long long durationMs = 0;
    int64_t id = 0;
};

// Records a time-ordered trace of shell activity: commands executed (with
// expanded args, pid, exit code, duration, captured output) and set -x style
// traces before each command runs. Surfaced in the dedicated Trace panel.
class TraceLog
{
public:
    TraceLog();

    void setEnabled(bool on);
    bool enabled() const;

    void record(TraceKind kind, std::wstring message);

    // Convenience for execution records with pid/exit/duration.
    void recordExecution(const std::wstring& command, uint32_t pid, int exitCode,
                         long long durationMs, const std::wstring& output);

    void clear();

    std::vector<TraceEvent> events() const;
    std::vector<TraceEvent> eventsOf(TraceKind kind) const;
    size_t count() const;

private:
    mutable std::mutex mtx_;
    std::vector<TraceEvent> events_;
    int64_t nextId_ = 1;
    bool enabled_ = false;
};

} // namespace kshell
