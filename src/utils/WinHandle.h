#pragma once

#ifndef KSHELL_WINHANDLE_H
#define KSHELL_WINHANDLE_H

#include <windows.h>

namespace kshell
{

class WinHandle
{
public:
    WinHandle() noexcept = default;
    explicit WinHandle(HANDLE handle) noexcept : handle_(handle) {}

    ~WinHandle()
    {
        close();
    }

    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;

    WinHandle(WinHandle&& other) noexcept : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    WinHandle& operator=(WinHandle&& other) noexcept
    {
        if (this != &other)
        {
            close();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    HANDLE get() const noexcept { return handle_; }
    HANDLE* addressOf() noexcept { return &handle_; }

    bool valid() const noexcept { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

    void release() noexcept { handle_ = nullptr; }

    void reset(HANDLE handle = nullptr) noexcept
    {
        if (handle_ != handle)
        {
            close();
            handle_ = handle;
        }
    }

private:
    void close() noexcept
    {
        if (valid())
        {
            ::CloseHandle(handle_);
        }
        handle_ = nullptr;
    }

    HANDLE handle_ = nullptr;
};

} // namespace kshell

#endif // KSHELL_WINHANDLE_H
