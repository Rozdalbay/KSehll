#pragma once

#ifndef KSHELL_HISTORY_H
#define KSHELL_HISTORY_H

#include <string>
#include <vector>

namespace kshell
{

class History
{
public:
    static constexpr int kDefaultMaxSize = 1000;

    explicit History(int maxSize = kDefaultMaxSize);

    bool loadFromFile(const std::wstring& filePath);
    bool saveToFile(const std::wstring& filePath) const;

    void add(const std::wstring& command);
    void clear();

    size_t size() const;
    bool empty() const;
    const std::vector<std::wstring>& entries() const;

    int maxSize() const;
    void setMaxSize(int maxSize);

private:
    std::vector<std::wstring> entries_;
    int maxSize_ = kDefaultMaxSize;
};

} // namespace kshell

#endif // KSHELL_HISTORY_H
