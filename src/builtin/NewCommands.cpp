#include "builtin/NewCommands.h"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <regex>

#include "core/ShellContext.h"
#include "utils/StringUtils.h"
#include "utils/PathUtils.h"
#include "process/ProcessManager.h"
#include "execution/Process.h"
#include "git/Git.h"

namespace fs = std::filesystem;

namespace kshell
{

namespace
{

std::wstring basenameOf(const std::wstring& p)
{
    return fs::path(p).filename().wstring();
}

double evalExpression(const std::wstring& expr, bool& ok)
{
    ok = true;
    std::wstring s = expr;
    // Strip whitespace.
    s.erase(std::remove_if(s.begin(), s.end(), [](wchar_t c) { return c == L' ' || c == L'\t'; }),
            s.end());
    if (s.empty())
    {
        ok = false;
        return 0;
    }

    // Simple recursive-descent: expr -> term {(+|-) term}; term -> factor {(*|/|%) factor}.
    size_t pos = 0;
    auto peek = [&]() -> wchar_t { return pos < s.size() ? s[pos] : 0; };
    std::function<double()> parseFactor, parseTerm, parseExpr;
    std::function<double()> parseNumber;
    parseNumber = [&]() -> double {
        size_t start = pos;
        std::wstring num;
        bool anyDot = false;
        while (pos < s.size())
        {
            wchar_t c = s[pos];
            if (std::iswdigit(c))
            {
                num += c;
                ++pos;
            }
            else if (c == L'.' && !anyDot)
            {
                anyDot = true;
                num += c;
                ++pos;
            }
            else
            {
                break;
            }
        }
        if (num.empty())
        {
            if (peek() == L'-')
            {
                ++pos;
                double v = parseNumber();
                return -v;
            }
            ok = false;
            return 0;
        }
        try
        {
            return std::stod(num);
        }
        catch (...)
        {
            ok = false;
            return 0;
        }
    };
    parseFactor = [&]() -> double {
        wchar_t c = peek();
        if (c == L'(')
        {
            ++pos;
            double v = parseExpr();
            if (peek() == L')')
            {
                ++pos;
            }
            else
            {
                ok = false;
            }
            return v;
        }
        return parseNumber();
    };
    parseTerm = [&]() -> double {
        double v = parseFactor();
        for (;;)
        {
            wchar_t c = peek();
            if (c == L'*')
            {
                ++pos;
                v *= parseFactor();
            }
            else if (c == L'/')
            {
                ++pos;
                double d = parseFactor();
                if (d == 0.0)
                {
                    ok = false;
                    return 0;
                }
                v /= d;
            }
            else if (c == L'%')
            {
                ++pos;
                double d = parseFactor();
                if (d == 0.0)
                {
                    ok = false;
                    return 0;
                }
                v = std::fmod(v, d);
            }
            else
            {
                break;
            }
        }
        return v;
    };
    parseExpr = [&]() -> double {
        double v = parseTerm();
        for (;;)
        {
            wchar_t c = peek();
            if (c == L'+')
            {
                ++pos;
                v += parseTerm();
            }
            else if (c == L'-')
            {
                ++pos;
                v -= parseTerm();
            }
            else
            {
                break;
            }
        }
        return v;
    };

    double result = parseExpr();
    if (pos != s.size())
    {
        ok = false;
    }
    return result;
}

std::wstring formatNumber(double v)
{
    std::wostringstream oss;
    oss.precision(10);
    if (std::floor(v) == v && std::abs(v) < 1e15)
    {
        oss << (long long)v;
    }
    else
    {
        oss << v;
    }
    return oss.str();
}

std::wstring humanBytes(uint64_t bytes)
{
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024.0 && u < 4)
    {
        v /= 1024.0;
        ++u;
    }
    wchar_t buf[64];
    swprintf(buf, 64, L"%.1f %ls", v, units[u]);
    return buf;
}

std::wstring quoteArg(const std::wstring& a)
{
    return a.find_first_of(L" \t") == std::wstring::npos ? a : (L"\"" + a + L"\"");
}

} // namespace

BuiltinResult builtinOpen(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    if (args.size() < 2)
    {
        ctx.printError(L"Usage: open <path|url|app> [args...]");
        return {};
    }
    std::wstring path = args[1];
    if (!pathutils::pathExists(path) && path.find(L"://") == std::wstring::npos)
    {
        // Try making it absolute relative to cwd.
        const std::wstring abs = fs::absolute(fs::path(path)).wstring();
        if (pathutils::pathExists(abs))
        {
            path = abs;
        }
    }
    HINSTANCE h = ::ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(h) <= 32)
    {
        ctx.printError(L"Could not open: " + path);
        return {};
    }
    return {};
}

BuiltinResult builtinTree(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    std::wstring dir = args.size() > 1 ? pathutils::expandPath(args[1]) : ctx.currentDirectory();
    if (!pathutils::isDirectory(dir))
    {
        ctx.printError(L"Not a directory: " + dir);
        return {};
    }
    int maxDepth = 0; // 0 = unlimited
    std::error_code ec;
    fs::path root(dir);
    ctx.printOutput(root.wstring());
    std::function<void(const fs::path&, int)> walk = [&](const fs::path& p, int depth) {
        if (maxDepth > 0 && depth > maxDepth)
        {
            return;
        }
        std::vector<fs::directory_entry> entries;
        for (const auto& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec))
        {
            entries.push_back(e);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const fs::directory_entry& a, const fs::directory_entry& b) {
                      return a.path().filename().wstring() < b.path().filename().wstring();
                  });
        for (const auto& e : entries)
        {
            std::wstring prefix(depth * 4, L' ');
            const std::wstring name = e.path().filename().wstring();
            if (e.is_directory(ec))
            {
                ctx.printOutput(prefix + L"[" + name + L"]");
                walk(e.path(), depth + 1);
            }
            else
            {
                ctx.printOutput(prefix + name);
            }
        }
    };
    walk(root, 0);
    return {};
}

BuiltinResult builtinWhere(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    if (args.size() < 2)
    {
        ctx.printError(L"Usage: where <command>");
        return {};
    }
    const std::wstring name = args[1];
    auto dirs = pathutils::getPathDirectories(ctx.environment().getPath());
    bool found = false;
    for (const auto& d : dirs)
    {
        for (const std::wstring& ext : std::vector<std::wstring>{L"", L".exe", L".com", L".bat", L".cmd", L".ps1"})
        {
            const fs::path p = fs::path(d) / (name + ext);
            if (fs::exists(p))
            {
                ctx.printOutput(p.wstring());
                found = true;
                break;
            }
        }
    }
    if (!found)
    {
        ctx.printError(L"Command not found: " + name);
    }
    return {};
}

BuiltinResult builtinFind(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    if (args.size() < 3)
    {
        ctx.printError(L"Usage: find <dir> <name>");
        return {};
    }
    std::wstring base = pathutils::expandPath(args[1]);
    std::wstring pattern = args[2];
    bool any = false;
    std::error_code ec;
    std::function<void(const fs::path&)> walk = [&](const fs::path& p) {
        std::error_code lec;
        for (const auto& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, lec))
        {
            std::wstring fname = e.path().filename().wstring();
            bool match = fname.find(pattern) != std::wstring::npos;
            if (!match)
            {
                try
                {
                    std::wregex re(pattern, std::regex_constants::icase);
                    match = std::regex_search(fname, re);
                }
                catch (...)
                {
                }
            }
            if (match)
            {
                ctx.printOutput(e.path().wstring());
                any = true;
            }
            if (e.is_directory(lec))
            {
                walk(e.path());
            }
        }
    };
    if (fs::exists(base))
    {
        walk(base);
    }
    if (!any)
    {
        ctx.printOutput(L"(no matches)");
    }
    return {};
}

BuiltinResult builtinSearch(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    if (args.size() < 2)
    {
        ctx.printError(L"Usage: search <text> [dir]");
        return {};
    }
    const std::wstring needle = args[1];
    std::wstring dir = args.size() > 2 ? pathutils::expandPath(args[2]) : ctx.currentDirectory();
    size_t hits = 0;
    std::error_code ec;
    std::function<void(const fs::path&)> walk = [&](const fs::path& p) {
        std::error_code lec;
        for (const auto& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, lec))
        {
            if (e.is_regular_file(lec))
            {
                const auto& path = e.path();
                if (path.extension() == L".exe" || path.extension() == L".dll")
                {
                    continue;
                }
                std::ifstream in(fs::path(path), std::ios::binary);
                if (!in)
                {
                    continue;
                }
                std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                std::wstring content;
                if (!bytes.empty())
                {
                    int wlen = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
                    if (wlen <= 0)
                    {
                        wlen = ::MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
                    }
                    if (wlen > 0)
                    {
                        content.resize((size_t)wlen);
                        ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), &content[0], wlen);
                    }
                }
                if (content.find(needle) != std::wstring::npos)
                {
                    ctx.printOutput(path.wstring());
                    ++hits;
                }
            }
            else if (e.is_directory(lec))
            {
                walk(e.path());
            }
        }
    };
    walk(dir);
    if (hits == 0)
    {
        ctx.printOutput(L"(no matches)");
    }
    return {};
}

BuiltinResult builtinSysinfo(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    (void)args;
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    ::GlobalMemoryStatusEx(&mem);

    SYSTEM_INFO si{};
    ::GetSystemInfo(&si);

    ctx.printOutput(L"OS: " + ctx.environment().get(L"OS").value_or(L"Windows"));
    ctx.printOutput(L"Computer: " + ctx.environment().getHostname());
    ctx.printOutput(L"User: " + ctx.environment().getUser());
    ctx.printOutput(L"Architecture: " + std::wstring(si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? L"x64" : L"x86"));
    ctx.printOutput(L"Processors: " + std::to_wstring(si.dwNumberOfProcessors) + L" logical");
    ctx.printOutput(L"RAM total: " + humanBytes(mem.ullTotalPhys) + L", used: " + humanBytes(mem.ullTotalPhys - mem.ullAvailPhys) +
                    L" (" + std::to_wstring(mem.dwMemoryLoad) + L"%)");
    ctx.printOutput(L"RAM page file: " + humanBytes(mem.ullTotalPageFile));
    ctx.printOutput(L"Uptime: " + std::to_wstring(::GetTickCount64() / 1000u) + L" s");
    return {};
}

BuiltinResult builtinProc(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    (void)args;
    process::ProcessManager pm;
    pm.primeCpuSample();
    ::Sleep(80);
    auto procs = pm.snapshot();
    int pid = args.size() > 1 ? _wtoi(args[1].c_str()) : 0;
    ctx.printOutput(L"PID\tName\t\t\tRAM");
    for (const auto& p : procs)
    {
        if (pid != 0 && (int)p.pid != pid)
        {
            continue;
        }
        ctx.printOutput(std::to_wstring(p.pid) + L"\t" + p.name + L"\t" + humanBytes((uint64_t)p.workingSetBytes));
    }
    return {};
}

BuiltinResult builtinTop(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    int count = args.size() > 1 ? _wtoi(args[1].c_str()) : 0;
    process::ProcessManager pm;
    pm.primeCpuSample();
    ::Sleep(120);
    auto procs = pm.snapshot();
    std::sort(procs.begin(), procs.end(),
              [](const process::ProcessInfo& a, const process::ProcessInfo& b) {
                  return a.cpuPercent > b.cpuPercent;
              });
    if (count <= 0 || (size_t)count > procs.size())
    {
        count = procs.size();
    }
    ctx.printOutput(L"PID    CPU%   RAM      Name");
    for (int i = 0; i < count; ++i)
    {
        const auto& p = procs[i];
        wchar_t line[256];
        swprintf(line, 256, L"%-6u %5.1f%%  %-8ls  %ls",
                 p.pid, p.cpuPercent, humanBytes((uint64_t)p.workingSetBytes).c_str(), p.name.c_str());
        ctx.printOutput(line);
    }
    return {};
}

BuiltinResult builtinCalc(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    if (args.size() < 2)
    {
        ctx.printError(L"Usage: calc <expression>   e.g. calc 2+3*4");
        return {};
    }
    std::wstring expr;
    for (size_t i = 1; i < args.size(); ++i)
    {
        expr += args[i];
        if (i + 1 < args.size())
        {
            expr += L" ";
        }
    }
    bool ok = false;
    double v = evalExpression(expr, ok);
    if (!ok)
    {
        ctx.printError(L"Invalid expression: " + expr);
        return {};
    }
    ctx.printOutput(expr + L" = " + formatNumber(v));
    return {};
}

BuiltinResult builtinJson(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    if (args.size() < 2)
    {
        ctx.printError(L"Usage: json <file.json>    (validate and pretty-print)");
        return {};
    }
    const std::wstring path = pathutils::expandPath(args[1]);
    const fs::path jsonPath(path);
    std::ifstream in(jsonPath, std::ios::binary);
    if (!in)
    {
        ctx.printError(L"Cannot open: " + path);
        return {};
    }
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::wstring text;
    if (!bytes.empty())
    {
        int wlen = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
        if (wlen <= 0)
        {
            wlen = ::MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
        }
        if (wlen > 0)
        {
            text.resize((size_t)wlen);
            ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), &text[0], wlen);
        }
    }

    // Minimal structural validator: count braces/brackets/strings balancing.
    int depth = 0;
    bool inStr = false;
    bool valid = true;
    for (size_t i = 0; i < text.size(); ++i)
    {
        wchar_t c = text[i];
        if (inStr)
        {
            if (c == L'\\')
            {
                ++i;
            }
            else if (c == L'"')
            {
                inStr = false;
            }
            continue;
        }
        if (c == L'"')
        {
            inStr = true;
        }
        else if (c == L'{' || c == L'[')
        {
            ++depth;
        }
        else if (c == L'}' || c == L']')
        {
            --depth;
            if (depth < 0)
            {
                valid = false;
                break;
            }
        }
    }
    if (inStr || depth != 0)
    {
        valid = false;
    }
    if (!valid)
    {
        ctx.printError(L"Invalid JSON structure in: " + path);
        return {};
    }

    // Pretty print: reindent with braces/brackets.
    std::wstring out;
    int indent = 0;
    inStr = false;
    for (size_t i = 0; i < text.size(); ++i)
    {
        wchar_t c = text[i];
        if (inStr)
        {
            out += c;
            if (c == L'\\' && i + 1 < text.size())
            {
                out += text[++i];
            }
            else if (c == L'"')
            {
                inStr = false;
            }
            continue;
        }
        if (c == L'"')
        {
            inStr = true;
            out += c;
        }
        else if (c == L'{' || c == L'[')
        {
            out += c;
            out += L'\n';
            ++indent;
            out.append((size_t)indent, L' ');
        }
        else if (c == L'}' || c == L']')
        {
            out += L'\n';
            --indent;
            if (indent < 0)
            {
                indent = 0;
            }
            out.append((size_t)indent, L' ');
            out += c;
        }
        else if (c == L',')
        {
            out += c;
            out += L'\n';
            out.append((size_t)indent, L' ');
        }
        else
        {
            out += c;
        }
    }
    ctx.printOutput(out);
    return {};
}

BuiltinResult builtinTheme(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    const render::ThemeManager* tm = ctx.themeManager();
    if (args.size() < 2)
    {
        if (tm)
        {
            ctx.printOutput(L"Themes:");
            for (size_t i = 0; i < tm->count(); ++i)
            {
                std::wstring mark = (i == (size_t)tm->activeIndex()) ? L"*" : L" ";
                ctx.printOutput(mark + L" " + tm->nameAt(i));
            }
        }
        else
        {
            ctx.printOutput(L"Current theme: " + ctx.config().themeName);
            ctx.printOutput(L"Themes: KShell Dark, KShell Light, High Contrast, Monochrome");
        }
        return {};
    }
    std::wstring name = args[1];
    ctx.config().themeName = name;
    ctx.config().save();
    if (ctx.onThemeChange)
    {
        ctx.onThemeChange(name);
        ctx.printSuccess(L"Theme set: " + name);
    }
    else
    {
        ctx.printOutput(L"Theme set in config: " + name + L" (restart to apply)");
    }
    return {};
}

BuiltinResult builtinReload(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    (void)args;
    ctx.config().load();
    ctx.history().loadFromFile(ctx.config().getHistoryFilePath());
    ctx.refreshExecutor();
    ctx.printSuccess(L"Configuration reloaded.");
    return {};
}

BuiltinResult builtinFg(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    int id = args.size() > 1 ? _wtoi(args[1].c_str()) : -1;
    auto& jobs = ctx.jobs();
    if (id < 0)
    {
        // Show the first running job.
        for (const auto& j : jobs.getAll())
        {
            if (j.state == JobState::Running)
            {
                id = j.id;
                break;
            }
        }
        if (id < 0)
        {
            ctx.printOutput(L"fg: no running jobs.");
            return {};
        }
    }
    auto found = jobs.findById(id);
    if (!found)
    {
        ctx.printError(L"fg: no such job: " + std::to_wstring(id));
        return {};
    }
    const Job* job = *found;
    if (job->state != JobState::Running)
    {
        ctx.printOutput(L"fg: job " + std::to_wstring(id) + L" is not running.");
        return {};
    }
    if (!job->processes.empty())
    {
        Process::wait(job->processes[0]);
        jobs.updateStates();
    }
    return {};
}

BuiltinResult builtinBg(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    int id = args.size() > 1 ? _wtoi(args[1].c_str()) : -1;
    auto found = ctx.jobs().findById(id);
    if (!found)
    {
        ctx.printError(L"bg: no such job: " + std::to_wstring(id));
        return {};
    }
    const Job* job = *found;
    ctx.printOutput(L"job " + std::to_wstring(job->id) + L" [" + job->command + L"] is running in background.");
    return {};
}

BuiltinResult builtinGit(ShellContext& ctx, const std::vector<std::wstring>& args)
{
    // `git` resolves as an external executable normally; this built-in passthrough
    // exists so `git` from a TUI pane still reaches the real git if a PATH entry
    // is not found. It just prints a hint and reports not-handled so the shell
    // falls through to external resolution.
    (void)ctx;
    (void)args;
    BuiltinResult r;
    r.handled = false;
    return r;
}

} // namespace kshell
