#include "git/Git.h"

#include <windows.h>

#include <cwctype>
#include <string>
#include <vector>

namespace kshell::git
{

namespace
{

// Run a command line in a directory, capture stdout+stderr into a buffer.
// Returns true if the process started and exited within the timeout.
bool runCapture(const std::wstring& cwd, const std::wstring& commandLine,
                std::wstring& out, int& exitCode, int timeoutMs)
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

    ::WaitForSingleObject(pi.hProcess, (DWORD)timeoutMs);
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

// Status letter -> readable label.
std::wstring statusLabel(wchar_t indexCode, wchar_t workCode, bool untracked)
{
    if (untracked)
    {
        return L"untracked";
    }
    wchar_t code = indexCode != L' ' ? indexCode : workCode;
    switch (code)
    {
    case L'M': return L"modified";
    case L'A': return L"added";
    case L'D': return L"deleted";
    case L'R': return L"renamed";
    case L'C': return L"copied";
    case L'U': return L"conflict";
    case L'?': return L"untracked";
    default:   return L"changed";
    }
}

// Split wide text into lines (keeps empty lines between \n).
std::vector<std::wstring> splitLines(const std::wstring& text)
{
    std::vector<std::wstring> lines;
    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t nl = text.find(L'\n', pos);
        if (nl == std::wstring::npos)
        {
            nl = text.size();
        }
        std::wstring line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }
        lines.push_back(line);
        pos = nl + 1;
    }
    return lines;
}

} // namespace

bool Git::gitAvailable() const
{
    std::wstring out;
    int code = 0;
    return runCapture(L"", L"git --version", out, code, 10000) && code == 0;
}

GitResult Git::runImpl(const std::wstring& dir, const std::wstring& argsLine,
                       int timeoutMs, bool /*logOutput*/)
{
    GitResult r;
    std::wstring out;
    int code = 0;
    if (!runCapture(dir, L"git " + argsLine, out, code, timeoutMs))
    {
        r.ok = false;
        return r;
    }
    r.ok = (code == 0);
    r.exitCode = code;
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

std::wstring Git::findRepoRoot(const std::wstring& dir)
{
    GitResult root = runImpl(dir, L"rev-parse --show-toplevel", 15000, false);
    if (!root.ok)
    {
        return L"";
    }
    return trimWS(root.stdoutText);
}

std::wstring Git::currentBranch(const std::wstring& dir)
{
    GitResult rev = runImpl(dir, L"rev-parse --abbrev-ref HEAD", 10000, false);
    if (!rev.ok || trimWS(rev.stdoutText) == L"HEAD")
    {
        return L"";
    }
    return trimWS(rev.stdoutText);
}

bool Git::isDetachedHead(const std::wstring& dir)
{
    GitResult sym = runImpl(dir, L"symbolic-ref -q HEAD", 10000, false);
    return !sym.ok;
}

GitStatus Git::status(const std::wstring& dir)
{
    GitStatus s;
    if (!gitAvailable())
    {
        return s;
    }

    GitResult top = runImpl(dir, L"rev-parse --show-toplevel", 15000, false);
    if (!top.ok)
    {
        return s;
    }
    s.isRepo = true;
    s.repoPath = trimWS(top.stdoutText);

    GitResult rev = runImpl(dir, L"rev-parse --abbrev-ref HEAD", 10000, false);
    if (rev.ok)
    {
        s.branch = trimWS(rev.stdoutText);
        if (s.branch == L"HEAD")
        {
            s.branch = L"(detached HEAD)";
            s.detachedHead = true;
        }
    }

    // Upstream + ahead/behind.
    GitResult up = runImpl(dir, L"for-each-ref --format=%(upstream:short) refs/heads/" + s.branch, 10000, false);
    GitResult ahead = runImpl(dir, L"rev-list --count @{upstream}..HEAD", 10000, false);
    if (ahead.ok)
    {
        s.ahead = parseInt(trimWS(ahead.stdoutText));
    }
    GitResult behind = runImpl(dir, L"rev-list --count HEAD..@{upstream}", 10000, false);
    if (behind.ok)
    {
        s.behind = parseInt(trimWS(behind.stdoutText));
    }

    // Remote name + URL.
    GitResult remote = runImpl(dir, L"remote", 10000, false);
    if (remote.ok)
    {
        std::vector<std::wstring> remotes = splitLines(remote.stdoutText);
        if (!s.branch.empty())
        {
            // Discover which remote tracks the current branch.
            GitResult cfgTrack = runImpl(dir,
                L"config --get branch." + s.branch + L".remote", 10000, false);
            if (cfgTrack.ok)
            {
                s.remoteName = trimWS(cfgTrack.stdoutText);
            }
        }
        if (s.remoteName.empty() && !remotes.empty())
        {
            s.remoteName = remotes[0];
        }
        if (!s.remoteName.empty())
        {
            GitResult url = runImpl(dir, L"config --get remote." + s.remoteName + L".url", 10000, false);
            if (url.ok)
            {
                s.remoteUrl = trimWS(url.stdoutText);
            }
        }
    }

    // Last commit summary.
    GitResult last = runImpl(dir,
        L"log -1 --format=%s%n%h%n%an%n%ai", 10000, false);
    if (last.ok)
    {
        auto lines = splitLines(last.stdoutText);
        if (lines.size() >= 4)
        {
            s.lastCommitSubject = lines[0];
            s.lastCommitShort = lines[1];
            s.lastCommitAuthor = lines[2];
            s.lastCommitDate = lines[3];
        }
    }

    // Per-file porcelain status (-z, NUL separated, no quoting).
    GitResult porc = runImpl(dir, L"status --porcelain -z", 15000, false);
    if (porc.ok)
    {
        size_t pos = 0;
        while (pos < porc.stdoutText.size())
        {
            size_t nz = porc.stdoutText.find(L'\0', pos);
            if (nz == std::wstring::npos)
            {
                nz = porc.stdoutText.size();
            }
            std::wstring field = porc.stdoutText.substr(pos, nz - pos);
            pos = (nz == porc.stdoutText.size()) ? nz + 1 : nz + 1;
            if (field.size() < 3)
            {
                continue;
            }
            GitFileEntry e;
            e.indexStatus = field[0];
            e.workTreeStatus = field[1];
            e.path = field.substr(3);
            bool untracked = (e.indexStatus == L'?');
            e.untracked = untracked;
            e.staged = !untracked && e.indexStatus != L' ';
            e.deleted = (e.indexStatus == L'D' || e.workTreeStatus == L'D');
            e.label = statusLabel(e.indexStatus, e.workTreeStatus, untracked);

            // Renames: porcelain -z adds a second NUL-terminated path then a
            // target path; for X? Keep the final path.
            if (e.indexStatus == L'R' || e.indexStatus == L'C')
            {
                size_t nz2 = pos; // next record
                // Grab next field
                size_t np = pos;
                size_t nz3 = porc.stdoutText.find(L'\0', np);
                if (nz3 == std::wstring::npos)
                {
                    nz3 = porc.stdoutText.size();
                }
                std::wstring target = porc.stdoutText.substr(np, nz3 - np);
                if (!target.empty())
                {
                    e.path += L" -> " + target;
                    pos = (nz3 == porc.stdoutText.size()) ? nz3 + 1 : nz3 + 1;
                }
            }

            if (untracked)
            {
                ++s.untracked;
            }
            else if (e.deleted)
            {
                ++s.deleted;
            }
            else
            {
                if (e.indexStatus != L' ')
                {
                    ++s.staged;
                }
                if (e.workTreeStatus != L' ')
                {
                    ++s.modified;
                }
            }
            s.fileEntries.push_back(e);
            s.changedFiles.push_back(e.path);
        }
    }

    return s;
}

std::vector<GitDiffLine> Git::parseDiff(const std::wstring& text)
{
    std::vector<GitDiffLine> diffLines;
    if (text.empty())
    {
        return diffLines;
    }
    auto lines = splitLines(text);
    int oldNo = 0;
    int newNo = 0;
    for (auto& line : lines)
    {
        GitDiffLine dl;
        if (line.size() >= 2 && line[0] == L'@' && line[1] == L'@')
        {
            dl.type = L'@';
            dl.text = line;
            // "@@ -oldStart,oldCount +newStart,newCount @@"
            size_t i = 2;
            while (i < line.size() && line[i] == L' ') ++i;
            if (i < line.size() && line[i] == L'-')
            {
                ++i;
                int os = 0;
                while (i < line.size() && iswdigit(line[i]))
                {
                    os = os * 10 + (line[i] - L'0');
                    ++i;
                }
                oldNo = os > 0 ? os : 0;
            }
            while (i < line.size() && line[i] != L'+') ++i;
            if (i < line.size() && line[i] == L'+')
            {
                ++i;
                int ns = 0;
                while (i < line.size() && iswdigit(line[i]))
                {
                    ns = ns * 10 + (line[i] - L'0');
                    ++i;
                }
                newNo = ns > 0 ? ns : 0;
            }
            diffLines.push_back(dl);
            continue;
        }
        if (line.empty())
        {
            dl.type = L' ';
            dl.text = line;
            diffLines.push_back(dl);
            continue;
        }
        wchar_t c = line[0];
        if (c == L' ')
        {
            dl.type = L' ';
            dl.oldNo = oldNo++;
            dl.newNo = newNo++;
            dl.text = line.substr(1);
        }
        else if (line == L"---")
        {
            dl.type = L'h';
            dl.text = line;
        }
        else if (line.size() >= 2 && line[0] == L'\\' && line[1] == L'\\')
        {
            dl.type = L'h';
            dl.text = line;
        }
        else if (c == L'-')
        {
            dl.type = L'-';
            dl.oldNo = oldNo++;
            dl.text = line.substr(1);
        }
        else if (c == L'+')
        {
            dl.type = L'+';
            dl.newNo = newNo++;
            dl.text = line.substr(1);
        }
        else
        {
            dl.type = L'h';
            dl.text = line;
        }
        diffLines.push_back(dl);
    }
    return diffLines;
}

std::vector<GitDiffLine> Git::diff(const std::wstring& dir, const std::wstring& path,
                                   bool cached)
{
    std::vector<std::wstring> args;
    args.push_back(L"diff");
    args.push_back(L"--no-color");
    args.push_back(L"--unified=3");
    if (cached)
    {
        args.push_back(L"--cached");
    }
    args.push_back(L"--");
    if (!path.empty())
    {
        args.push_back(path);
    }
    GitResult res = run(dir, args);
    return parseDiff(res.stdoutText);
}

std::wstring Git::diffStat(const std::wstring& dir, bool cached)
{
    std::vector<std::wstring> args{ L"diff", L"--stat" };
    if (cached)
    {
        args.push_back(L"--cached");
    }
    GitResult res = run(dir, args);
    return res.ok ? trimWS(res.stdoutText) : L"";
}

std::vector<GitCommit> Git::log(const std::wstring& dir, int limit)
{
    std::vector<GitCommit> commits;
    if (limit <= 0)
    {
        limit = 30;
    }
    const std::wstring fmt =
        L"%H%x1f%h%x1f%s%x1f%b%x1f%an%x1f%ae%x1f%ad%x1f%ar%x1f%D";
    GitResult res = runImpl(dir,
        L"log --no-color --date=iso --decorate -z --pretty=format:" + fmt +
        L" -n " + std::to_wstring(limit),
        20000, false);
    if (!res.ok)
    {
        return commits;
    }

    // Records are separated by NUL, fields by 0x1f.
    size_t pos = 0;
    while (pos < res.stdoutText.size())
    {
        size_t nz = res.stdoutText.find(L'\0', pos);
        if (nz == std::wstring::npos)
        {
            nz = res.stdoutText.size();
        }
        std::wstring record = res.stdoutText.substr(pos, nz - pos);
        pos = nz + 1;
        if (record.empty())
        {
            continue;
        }
        // Split fields by 0x1f.
        std::vector<std::wstring> f;
        size_t fp = 0;
        while (fp <= record.size())
        {
            size_t fs = record.find(0x1f, fp);
            if (fs == std::wstring::npos)
            {
                fs = record.size();
            }
            f.push_back(record.substr(fp, fs - fp));
            fp = fs + 1;
        }
        if (f.size() < 9)
        {
            continue;
        }
        GitCommit c;
        c.hash = f[0];
        c.shortHash = f[1];
        c.subject = f[2];
        c.body = f[3];
        c.author = f[4];
        c.authorEmail = f[5];
        c.date = f[6];
        c.relativeDate = f[7];
        c.branches = f[8];
        if (c.branches == L"HEAD -> " + c.shortHash || c.branches.find(L"HEAD -> ") == 0)
        {
            c.isHead = true;
        }
        commits.push_back(std::move(c));
    }
    return commits;
}

GitResult Git::show(const std::wstring& dir, const std::wstring& rev)
{
    return runImpl(dir, L"show --stat --oneline --no-color " + rev, 20000, false);
}

std::wstring Git::showText(const std::wstring& dir, const std::wstring& rev)
{
    GitResult res = runImpl(dir, L"show --no-color " + rev, 20000, false);
    return res.ok ? res.stdoutText : res.stderrText;
}

std::vector<GitBranch> Git::localBranches(const std::wstring& dir)
{
    std::vector<GitBranch> out;
    GitResult res = runImpl(dir,
        L"for-each-ref --format=%(objectname:short)%00%(refname:short)%00%(upstream:short)%00%(upstream:track) refs/heads",
        15000, false);
    if (!res.ok)
    {
        return out;
    }
    auto lines = splitLines(res.stdoutText);
    for (auto& line : lines)
    {
        if (line.empty())
        {
            continue;
        }
        GitBranch b;
        size_t p0 = line.find(L'\0');
        if (p0 == std::wstring::npos)
        {
            continue;
        }
        b.lastCommit = line.substr(0, p0);
        size_t p1 = line.find(L'\0', p0 + 1);
        if (p1 == std::wstring::npos)
        {
            continue;
        }
        b.name = line.substr(p0 + 1, p1 - p0 - 1);
        size_t p2 = line.find(L'\0', p1 + 1);
        if (p2 != std::wstring::npos)
        {
            b.upstream = line.substr(p1 + 1, p2 - p1 - 1);
            std::wstring track = line.substr(p2 + 1);
            if (!track.empty())
            {
                // "[ahead 1]", "[behind 2]", "[ahead 1, behind 2]"
                size_t ga = track.find(L"ahead ");
                size_t gb = track.find(L"behind ");
                if (ga != std::wstring::npos)
                {
                    size_t e = track.find_first_of(L" ,]", ga + 6);
                    b.ahead = parseInt(track.substr(ga + 6, e - ga - 6));
                }
                if (gb != std::wstring::npos)
                {
                    size_t e = track.find_first_of(L" ,]", gb + 7);
                    b.behind = parseInt(track.substr(gb + 7, e - gb - 7));
                }
            }
        }
        out.push_back(std::move(b));
    }
    std::wstring cur = currentBranch(dir);
    for (auto& b : out)
    {
        b.isCurrent = (b.name == cur && !cur.empty());
    }
    return out;
}

std::vector<GitBranch> Git::remoteBranches(const std::wstring& dir)
{
    std::vector<GitBranch> out;
    GitResult res = runImpl(dir,
        L"for-each-ref --format=%(objectname:short)%00%(refname:short) refs/remotes",
        15000, false);
    if (!res.ok)
    {
        return out;
    }
    auto lines = splitLines(res.stdoutText);
    for (auto& line : lines)
    {
        if (line.empty())
        {
            continue;
        }
        size_t p0 = line.find(L'\0');
        if (p0 == std::wstring::npos)
        {
            continue;
        }
        GitBranch b;
        b.lastCommit = line.substr(0, p0);
        b.name = line.substr(p0 + 1);
        b.isRemote = true;
        out.push_back(std::move(b));
    }
    return out;
}

std::vector<GitBranch> Git::allBranches(const std::wstring& dir)
{
    auto locals = localBranches(dir);
    auto remotes = remoteBranches(dir);
    locals.insert(locals.end(), remotes.begin(), remotes.end());
    return locals;
}

std::vector<GitRemote> Git::remotes(const std::wstring& dir)
{
    std::vector<GitRemote> out;
    GitResult res = runImpl(dir, L"remote", 10000, false);
    if (!res.ok)
    {
        return out;
    }
    auto names = splitLines(res.stdoutText);
    for (auto& name : names)
    {
        if (name.empty())
        {
            continue;
        }
        GitRemote r;
        r.name = name;
        GitResult url = runImpl(dir, L"config --get remote." + name + L".url", 10000, false);
        if (url.ok)
        {
            r.url = trimWS(url.stdoutText);
        }
        GitResult furl = runImpl(dir, L"config --get remote." + name + L".fetch", 10000, false);
        if (furl.ok)
        {
            r.fetchUrl = trimWS(furl.stdoutText);
        }
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<GitStashEntry> Git::stashList(const std::wstring& dir)
{
    std::vector<GitStashEntry> out;
    GitResult res = runImpl(dir, L"stash list", 10000, false);
    if (!res.ok)
    {
        return out;
    }
    auto lines = splitLines(res.stdoutText);
    int index = 0;
    for (auto& line : lines)
    {
        if (line.empty())
        {
            continue;
        }
        GitStashEntry e;
        e.index = index;
        // Modern format: "stash@{0}: On main: commit message"
        // Legacy format: "stash@{0}: WIP on main: abc1234 message"
        size_t colon = line.find(L':');
        if (colon != std::wstring::npos)
        {
            e.label = line.substr(0, colon);
            std::wstring rest = line.substr(colon + 1);
            while (!rest.empty() && rest.front() == L' ') rest.erase(rest.begin());
            e.message = rest;
        }
        else
        {
            e.label = line;
        }
        index++;
        out.push_back(std::move(e));
    }
    return out;
}

std::wstring Git::graphText(const std::wstring& dir, int limit)
{
    if (limit <= 0)
    {
        limit = 60;
    }
    GitResult res = runImpl(dir,
        L"log --graph --oneline --decorate --all -n " + std::to_wstring(limit),
        20000, false);
    return res.ok ? res.stdoutText : L"";
}

} // namespace kshell::git