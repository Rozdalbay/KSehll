#include "git/Git.h"

#include <windows.h>

#include <string>
#include <vector>

namespace kshell::git
{

namespace
{

// Run a command line in a directory, capture stdout+stderr into a buffer.
// Returns true if the process started.
bool runCapture(const std::wstring& cwd, const std::wstring& commandLine,
                std::wstring& out, int& exitCode)
{
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!::CreatePipe(&readPipe, &writePipe, &sa, 0))
    {
        return false;
    }
    ::SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
    cmd.push_back(L'\0');

    BOOL ok = ::CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                               CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                               nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    ::CloseHandle(writePipe);
    if (!ok)
    {
        ::CloseHandle(readPipe);
        return false;
    }

    // Read all available output.
    std::string bytes;
    char buf[4096];
    DWORD read = 0;
    for (;;)
    {
        if (!::ReadFile(readPipe, buf, sizeof(buf), &read, nullptr) || read == 0)
        {
            break;
        }
        bytes.append(buf, read);
    }
    ::CloseHandle(readPipe);

    ::WaitForSingleObject(pi.hProcess, 6000);
    DWORD code = 0;
    ::GetExitCodeProcess(pi.hProcess, &code);
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);

    std::wstring result;
    if (!bytes.empty())
    {
        int wlen = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
        if (wlen <= 0)
        {
            wlen = ::MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
        }
        if (wlen > 0)
        {
            result.resize((size_t)wlen);
            MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), &result[0], wlen);
        }
    }

    out = std::move(result);
    exitCode = (int)code;
    return true;
}

std::wstring trimWS(std::wstring s)
{
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos)
    {
        return L"";
    }
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

int parseInt(const std::wstring& s)
{
    try
    {
        return std::stoi(s);
    }
    catch (...)
    {
        return 0;
    }
}

} // namespace

bool Git::gitAvailable() const
{
    std::wstring out;
    int code = 0;
    return runCapture(L"", L"git --version", out, code) && code == 0;
}

GitResult Git::runImpl(const std::wstring& dir, const std::wstring& argsLine)
{
    GitResult r;
    std::wstring out;
    int code = 0;
    if (!runCapture(dir, L"git " + argsLine, out, code))
    {
        r.ok = false;
        return r;
    }
    r.ok = (code == 0);
    r.exitCode = code;
    // Best effort: split captured text; assume single stream (stderr merged).
    r.stdoutText = out;
    return r;
}

GitResult Git::runSimple(const std::wstring& dir, const std::wstring& argsLine)
{
    return runImpl(dir, argsLine);
}

GitResult Git::run(const std::wstring& dir, const std::vector<std::wstring>& args)
{
    std::wstring line;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0)
        {
            line += L" ";
        }
        const auto& a = args[i];
        bool needsQuote = a.find_first_of(L" \t\r\n\"") != std::wstring::npos;
        if (needsQuote)
        {
            line += L'"';
            for (wchar_t c : a)
            {
                if (c == L'"')
                {
                    line += L"\\\"";
                }
                else
                {
                    line += c;
                }
            }
            line += L'"';
        }
        else
        {
            line += a;
        }
    }
    return runImpl(dir, line);
}

GitStatus Git::status(const std::wstring& dir)
{
    GitStatus s;
    if (!gitAvailable())
    {
        return s;
    }

    GitResult rev = runImpl(dir, L"rev-parse --abbrev-ref HEAD");
    if (!rev.ok)
    {
        return s;
    }
    s.isRepo = true;
    s.branch = trimWS(rev.stdoutText);

    // repo top-level
    GitResult top = runImpl(dir, L"rev-parse --show-toplevel");
    if (top.ok)
    {
        s.repoPath = trimWS(top.stdoutText);
    }

    GitResult ahead = runImpl(dir, L"rev-list --count @{upstream}..HEAD");
    if (ahead.ok)
    {
        s.ahead = parseInt(trimWS(ahead.stdoutText));
    }
    GitResult behind = runImpl(dir, L"rev-list --count HEAD..@{upstream}");
    if (behind.ok)
    {
        s.behind = parseInt(trimWS(behind.stdoutText));
    }

    GitResult porc = runImpl(dir, L"status --porcelain");
    if (porc.ok)
    {
        std::wstring text = trimWS(porc.stdoutText);
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t nl = text.find(L'\n', pos);
            if (nl == std::wstring::npos)
            {
                nl = text.size();
            }
            std::wstring line = text.substr(pos, nl - pos);
            pos = nl + 1;
            if (line.size() < 3)
            {
                continue;
            }
            const wchar_t x = line[0];
            const wchar_t y = line[1];
            if (x != L' ' && x != L'?')
            {
                ++s.staged;
            }
            if (x == L'?')
            {
                ++s.untracked;
            }
            else if (y != L' ')
            {
                ++s.modified;
            }
            if (line.size() > 3)
            {
                std::wstring path = line.substr(3);
                // strip quotes
                if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"')
                {
                    path = path.substr(1, path.size() - 2);
                }
                s.changedFiles.push_back(path);
            }
        }
    }

    return s;
}

} // namespace kshell::git
