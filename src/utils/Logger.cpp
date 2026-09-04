#include "utils/Logger.h"

#include <windows.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace kshell
{

Logger::Logger()
{
}

void Logger::open(const std::wstring& filePath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open())
    {
        file_.close();
    }

    std::error_code ec;
    const std::filesystem::path parent = std::filesystem::path(filePath).parent_path();
    std::filesystem::create_directories(parent, ec);

    file_.open(filePath.c_str(), std::ios::out | std::ios::app);
}

void Logger::close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open())
    {
        file_.flush();
        file_.close();
    }
}

const wchar_t* Logger::levelName(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug:   return L"DEBUG";
    case LogLevel::Info:    return L"INFO";
    case LogLevel::Warning: return L"WARNING";
    case LogLevel::Error:   return L"ERROR";
    }
    return L"INFO";
}

std::wstring Logger::timestamp() const
{
    const auto now = std::chrono::system_clock::now();
    const auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf = {};
    localtime_s(&tmBuf, &timeT);

    std::wstringstream ss;
    ss << std::put_time(&tmBuf, L"%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Logger::write(LogLevel level, const std::wstring& message)
{
    if (static_cast<int>(level) < static_cast<int>(level_))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open())
    {
        return;
    }
    file_ << L"[" << timestamp() << L"] [" << levelName(level) << L"] " << message << L"\n";
    file_.flush();
}

} // namespace kshell
