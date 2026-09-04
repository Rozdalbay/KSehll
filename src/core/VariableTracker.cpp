#include "core/VariableTracker.h"

#include <windows.h>

#include <ctime>

namespace kshell
{

VariableTracker::VariableTracker() = default;

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

void VariableTracker::recordSet(const std::wstring& name, const std::wstring& value)
{
    std::lock_guard<std::mutex> lk(mtx_);
    auto& v = variables_[name];
    v.name = name;
    v.userSet = true;
    v.currentValue = value;
    v.history.push_back({value, nowString()});
    if (v.history.size() > 200)
    {
        v.history.erase(v.history.begin());
    }
}

void VariableTracker::recordUnset(const std::wstring& name)
{
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = variables_.find(name);
    if (it == variables_.end())
    {
        return;
    }
    it->second.history.push_back({L"<unset>", nowString()});
    it->second.currentValue.clear();
    variables_.erase(it);
}

std::vector<TrackedVariable> VariableTracker::snapshot() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<TrackedVariable> result;
    result.reserve(variables_.size());
    for (const auto& [_, v] : variables_)
    {
        result.push_back(v);
    }
    return result;
}

const std::vector<VariableChange>& VariableTracker::historyFor(const std::wstring& name) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    static const std::vector<VariableChange> empty;
    const auto it = variables_.find(name);
    return it == variables_.end() ? empty : it->second.history;
}

bool VariableTracker::empty() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return variables_.empty();
}

} // namespace kshell
