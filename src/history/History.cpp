#include "history/History.h"

#include <fstream>
#include <filesystem>
#include <system_error>

#include "utils/StringUtils.h"

namespace fs = std::filesystem;

namespace kshell
{

History::History(int maxSize)
    : maxSize_(maxSize)
{
}

bool History::loadFromFile(const std::wstring& filePath)
{
    std::error_code ec;
    if (!fs::exists(fs::path(filePath), ec))
    {
        return false;
    }

    std::wifstream file(filePath.c_str());
    if (!file.is_open())
    {
        return false;
    }

    entries_.clear();
    std::wstring line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }
        if (!stringutils::trim(line).empty())
        {
            entries_.push_back(line);
        }
    }

    if (static_cast<int>(entries_.size()) > maxSize_)
    {
        entries_.erase(entries_.begin(), entries_.end() - maxSize_);
    }
    return true;
}

bool History::saveToFile(const std::wstring& filePath) const
{
    std::error_code ec;
    const fs::path parent = fs::path(filePath).parent_path();
    fs::create_directories(parent, ec);

    std::wofstream file(filePath.c_str(), std::ios::trunc);
    if (!file.is_open())
    {
        return false;
    }
    for (const auto& entry : entries_)
    {
        file << entry << L"\n";
    }
    return true;
}

void History::add(const std::wstring& command)
{
    const std::wstring trimmed = stringutils::trim(command);
    if (trimmed.empty())
    {
        return;
    }
    if (!entries_.empty() && entries_.back() == trimmed)
    {
        return;
    }
    entries_.push_back(trimmed);
    if (static_cast<int>(entries_.size()) > maxSize_)
    {
        entries_.erase(entries_.begin());
    }
}

void History::clear()
{
    entries_.clear();
}

size_t History::size() const
{
    return entries_.size();
}

bool History::empty() const
{
    return entries_.empty();
}

const std::vector<std::wstring>& History::entries() const
{
    return entries_;
}

int History::maxSize() const
{
    return maxSize_;
}

void History::setMaxSize(int maxSize)
{
    maxSize_ = maxSize;
    if (maxSize_ <= 0)
    {
        maxSize_ = kDefaultMaxSize;
    }
    if (static_cast<int>(entries_.size()) > maxSize_)
    {
        entries_.erase(entries_.begin(), entries_.end() - maxSize_);
    }
}

} // namespace kshell
