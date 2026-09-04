#pragma once

#ifndef KSHELL_LOGGER_H
#define KSHELL_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

namespace kshell
{

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

class Logger
{
public:
    Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void open(const std::wstring& filePath);

    void close();

    void setLevel(LogLevel level) { level_ = level; }
    LogLevel level() const { return level_; }

    bool isOpen() const { return file_.is_open(); }

    void debug(const std::wstring& message) { write(LogLevel::Debug, message); }
    void info(const std::wstring& message) { write(LogLevel::Info, message); }
    void warning(const std::wstring& message) { write(LogLevel::Warning, message); }
    void error(const std::wstring& message) { write(LogLevel::Error, message); }

    static const wchar_t* levelName(LogLevel level);

private:
    void write(LogLevel level, const std::wstring& message);
    std::wstring timestamp() const;

    std::wofstream file_;
    std::mutex mutex_;
    LogLevel level_ = LogLevel::Debug;
};

} // namespace kshell

#endif // KSHELL_LOGGER_H
