#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace kshell
{

struct VariableChange
{
    std::wstring value;
    std::wstring timestamp;
};

struct TrackedVariable
{
    std::wstring name;
    std::wstring currentValue;
    std::vector<VariableChange> history;
    bool userSet = false;
};

// Tracks user-defined shell variables set via `set`/`unset`, recording each
// change with a timestamp so the UI can show both the current value and the
// change history. `set` still writes to the real OS environment; this store
// simply observes and records those changes.
class VariableTracker
{
public:
    VariableTracker();

    // Record a variable being set/updated.
    void recordSet(const std::wstring& name, const std::wstring& value);

    // Record a variable being removed.
    void recordUnset(const std::wstring& name);

    // Snapshot of all tracked variables (current values).
    std::vector<TrackedVariable> snapshot() const;

    // Full change history for one variable.
    const std::vector<VariableChange>& historyFor(const std::wstring& name) const;

    bool empty() const;

private:
    mutable std::mutex mtx_;
    std::map<std::wstring, TrackedVariable> variables_;
};

} // namespace kshell
