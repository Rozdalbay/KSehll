#include "builtin/BuiltinCommand.h"
#include "core/ShellContext.h"

#include "utils/StringUtils.h"
#include "utils/PathUtils.h"
#include "utils/ErrorUtils.h"

#include <windows.h>

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace fs = std::filesystem;

namespace kshell
{

static std::wstring formatFileSize(ULONGLONG size)
{
    if (size < 1024)
    {
        return std::to_wstring(size) + L" B";
    }
    else if (size < 1024ULL * 1024)
    {
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(1) << size / 1024.0 << L" KB";
        return ss.str();
    }
    else if (size < 1024ULL * 1024 * 1024)
    {
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(1) << size / (1024.0 * 1024.0) << L" MB";
        return ss.str();
    }
    else
    {
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(1) << size / (1024.0 * 1024.0 * 1024.0) << L" GB";
        return ss.str();
    }
}

BuiltinResult builtinDir(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    bool showAll = false;
    bool showLong = false;
    std::wstring target = ctx.currentDirectory();

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring& arg = args[i];
        if (arg[0] == L'-' || arg[0] == L'/')
        {
            for (size_t j = 1; j < arg.size(); ++j)
            {
                if (arg[j] == L'a' || arg[j] == L'A')
                    showAll = true;
                else if (arg[j] == L'l' || arg[j] == L'L')
                    showLong = true;
            }
        }
        else
        {
            target = pathutils::expandPath(arg);
        }
    }

    if (!pathutils::pathExists(target) || !pathutils::isDirectory(target))
    {
        ctx.printError(L"dir: cannot access:\n" + target);
        result.exitCode = 1;
        return result;
    }

    std::error_code ec;
    if (showLong)
    {
        int count = 0;
        for (const auto& entry : fs::directory_iterator(target, ec))
        {
            const std::wstring name = entry.path().filename().wstring();
            if (!showAll && (name == L"." || name == L".."))
            {
                continue;
            }
            const DWORD attrs = ::GetFileAttributesW(entry.path().c_str());
            auto modTime = entry.last_write_time(ec);
            auto systemTime = std::chrono::file_clock::to_sys(modTime);
            auto timeT = std::chrono::system_clock::to_time_t(systemTime);
            std::wstringstream timeStr;
            std::tm tmBuf = {};
            localtime_s(&tmBuf, &timeT);
            timeStr << std::put_time(&tmBuf, L"%Y-%m-%d %H:%M");
            std::wstring sizeStr;
            if (attrs & FILE_ATTRIBUTE_DIRECTORY)
            {
                sizeStr = L"   <DIR>   ";
            }
            else
            {
                std::error_code ec2;
                auto fileSize = entry.file_size(ec2);
                sizeStr = formatFileSize(fileSize);
                while (sizeStr.size() < 12)
                {
                    sizeStr = L" " + sizeStr;
                }
            }
            std::wstring line = timeStr.str() + L"   " + sizeStr + L" " + name;
            ctx.printOutput(line);
            ++count;
        }
        ctx.printOutput(L"\nTotal: " + std::to_wstring(count) + L" items");
    }
    else
    {
        std::wstring output;
        int count = 0;
        for (const auto& entry : fs::directory_iterator(target, ec))
        {
            const std::wstring name = entry.path().filename().wstring();
            if (!showAll && (name == L"." || name == L".."))
            {
                continue;
            }
            if (!output.empty())
            {
                output += L"  ";
            }
            output += name;
            ++count;
        }
        ctx.printOutput(output);
    }

    result.exitCode = 0;
    return result;
}

BuiltinResult builtinLs(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    bool showAll = false;
    bool showLong = false;
    std::wstring target = L".";

    for (size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == L"-a" || args[i] == L"-al" || args[i] == L"-la")
        {
            showAll = true;
            showLong = (args[i] == L"-al" || args[i] == L"-la");
        }
        else if (args[i] == L"-l")
        {
            showLong = true;
        }
        else
        {
            target = args[i];
        }
    }

    std::vector<std::wstring> newArgs = {L"dir"};
    if (showAll)
        newArgs.push_back(L"-a");
    if (showLong)
        newArgs.push_back(L"-l");
    if (target != L".")
        newArgs.push_back(target);

    return builtinDir(ctx, newArgs);
}

BuiltinResult builtinMkdir(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    if (args.size() < 2)
    {
        ctx.printError(L"mkdir: missing argument");
        result.exitCode = 1;
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring path = pathutils::expandPath(args[i]);
        std::error_code ec;
        fs::create_directories(fs::path(path), ec);
        if (ec)
        {
            ctx.printError(L"mkdir: cannot create directory:\n" + path);
            result.exitCode = 1;
        }
    }
    return result;
}

BuiltinResult builtinRmdir(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    if (args.size() < 2)
    {
        ctx.printError(L"rmdir: missing argument");
        result.exitCode = 1;
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring path = pathutils::expandPath(args[i]);
        std::error_code ec;
        fs::remove(fs::path(path), ec);
        if (ec)
        {
            ctx.printError(L"rmdir: cannot remove:\n" + path);
            result.exitCode = 1;
        }
    }
    return result;
}

BuiltinResult builtinTouch(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    if (args.size() < 2)
    {
        ctx.printError(L"touch: missing argument");
        result.exitCode = 1;
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring path = pathutils::expandPath(args[i]);
        HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            ctx.printError(L"touch: cannot create file:\n" + path);
            result.exitCode = 1;
        }
        else
        {
            ::CloseHandle(h);
        }
    }
    return result;
}

BuiltinResult builtinCopy(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    if (args.size() < 3)
    {
        ctx.printError(L"copy: missing source or destination");
        result.exitCode = 1;
        return result;
    }

    const std::wstring src = pathutils::expandPath(args[1]);
    const std::wstring dst = pathutils::expandPath(args[2]);
    BOOL ok = ::CopyFileW(src.c_str(), dst.c_str(), FALSE);
    if (!ok)
    {
        ctx.printError(L"copy: failed to copy:\n" + src + L"\nto:\n" + dst);
        result.exitCode = 1;
    }
    return result;
}

BuiltinResult builtinMove(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    if (args.size() < 3)
    {
        ctx.printError(L"move: missing source or destination");
        result.exitCode = 1;
        return result;
    }

    const std::wstring src = pathutils::expandPath(args[1]);
    const std::wstring dst = pathutils::expandPath(args[2]);
    BOOL ok = ::MoveFileW(src.c_str(), dst.c_str());
    if (!ok)
    {
        ctx.printError(L"move: failed to move:\n" + src + L"\nto:\n" + dst);
        result.exitCode = 1;
    }
    return result;
}

BuiltinResult builtinDel(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    if (args.size() < 2)
    {
        ctx.printError(L"del: missing argument");
        result.exitCode = 1;
        return result;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::wstring path = pathutils::expandPath(args[i]);
        BOOL ok = ::DeleteFileW(path.c_str());
        if (!ok)
        {
            ctx.printError(L"del: cannot delete:\n" + path);
            result.exitCode = 1;
        }
    }
    return result;
}

BuiltinResult builtinType(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    BuiltinResult result;
    if (args.size() < 2)
    {
        ctx.printError(L"type: missing argument");
        result.exitCode = 1;
        return result;
    }

    const std::wstring path = pathutils::expandPath(args[1]);
    std::wifstream file(path.c_str());
    if (!file.is_open())
    {
        ctx.printError(L"type: cannot open file:\n" + path);
        result.exitCode = 1;
        return result;
    }

    std::wstring line;
    while (std::getline(file, line))
    {
        ctx.printOutput(line);
    }
    return result;
}

BuiltinResult builtinCat(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    return builtinType(ctx, args);
}

} // namespace kshell