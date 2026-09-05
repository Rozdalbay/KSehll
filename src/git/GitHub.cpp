#include "git/GitHub.h"

#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp")

#include <shlobj.h>

#include <cwctype>
#include <string>
#include <vector>

namespace kshell::github
{

namespace
{

std::wstring trim(const std::wstring& s)
{
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return L"";
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

// True when `s` contains a top-level object with the given key (cheap scan,
// used to detect the "pull_request" field presence).
bool keyExists(const std::wstring& s, const std::wstring& key)
{
    return s.find(L"\"" + key + L"\"") != std::wstring::npos;
}

// Returns the string value of `"key": "..."` (with escapes basic only).
std::wstring jsonStr(const std::wstring& s, const std::wstring& key)
{
    std::wstring needle = L"\"" + key + L"\"";
    size_t pos = s.find(needle);
    if (pos == std::wstring::npos) return L"";
    pos += needle.size();
    while (pos < s.size() && (s[pos] == L' ' || s[pos] == L':')) ++pos;
    if (pos >= s.size() || s[pos] != L'"') return L"";
    ++pos;
    std::wstring out;
    while (pos < s.size())
    {
        wchar_t c = s[pos++];
        if (c == L'"') break;
        if (c == L'\\' && pos < s.size())
        {
            wchar_t e = s[pos++];
            if (e == L'n') out += L'\n';
            else if (e == L't') out += L'\t';
            else if (e == L'u' && pos + 4 <= s.size())
            {
                out += s.substr(pos, 4);
                pos += 4;
            }
            else out += e;
            continue;
        }
        out += c;
    }
    return out;
}

bool jsonBool(const std::wstring& s, const std::wstring& key)
{
    std::wstring needle = L"\"" + key + L"\"";
    size_t pos = s.find(needle);
    if (pos == std::wstring::npos) return false;
    pos += needle.size();
    while (pos < s.size() && (s[pos] == L' ' || s[pos] == L':')) ++pos;
    if (s.compare(pos, 4, L"true") == 0) return true;
    if (s.compare(pos, 5, L"false") == 0) return false;
    return false;
}

long jsonInt(const std::wstring& s, const std::wstring& key)
{
    std::wstring needle = L"\"" + key + L"\"";
    size_t pos = s.find(needle);
    if (pos == std::wstring::npos) return 0;
    pos += needle.size();
    while (pos < s.size() && (s[pos] == L' ' || s[pos] == L':')) ++pos;
    long v = 0;
    bool neg = false;
    if (pos < s.size() && s[pos] == L'-') { neg = true; ++pos; }
    while (pos < s.size() && iswdigit(s[pos])) v = v * 10 + (s[pos++] - L'0');
    return neg ? -v : v;
}

// Splits a JSON array into its top-level object chunks (brace-balanced,
// string aware).
std::vector<std::wstring> jsonObjects(const std::wstring& s)
{
    std::vector<std::wstring> out;
    size_t pos = 0;
    while (pos < s.size() && s[pos] != L'[') ++pos;
    if (pos == s.size()) return out;
    ++pos;
    while (pos < s.size())
    {
        while (pos < s.size() && (s[pos] == L' ' || s[pos] == L',' || s[pos] == L'\n'))
            ++pos;
        if (pos >= s.size() || s[pos] == L']') break;
        if (s[pos] == L'{')
        {
            int depth = 0;
            bool inStr = false;
            size_t start = pos;
            while (pos < s.size())
            {
                wchar_t c = s[pos];
                if (inStr)
                {
                    if (c == L'\\') { pos += 2; continue; }
                    if (c == L'"') inStr = false;
                }
                else if (c == L'"') inStr = true;
                else if (c == L'{') ++depth;
                else if (c == L'}') { --depth; if (depth == 0) { ++pos; break; } }
                ++pos;
            }
            out.push_back(s.substr(start, pos - start));
        }
        else
        {
            // skip scalar
            while (pos < s.size() && s[pos] != L',' && s[pos] != L']') ++pos;
        }
    }
    return out;
}

// Extracts the value of `"key": {...}` as a raw JSON object chunk, or empty.
std::wstring jsonObject(const std::wstring& s, const std::wstring& key)
{
    std::wstring needle = L"\"" + key + L"\"";
    size_t pos = s.find(needle);
    if (pos == std::wstring::npos) return L"";
    pos += needle.size();
    while (pos < s.size() && (s[pos] == L' ' || s[pos] == L':')) ++pos;
    if (pos >= s.size() || s[pos] != L'{') return L"";
    size_t start = pos;
    int depth = 0;
    bool inStr = false;
    while (pos < s.size())
    {
        wchar_t c = s[pos];
        if (inStr)
        {
            if (c == L'\\') { pos += 2; continue; }
            if (c == L'"') inStr = false;
        }
        else if (c == L'"') inStr = true;
        else if (c == L'{') ++depth;
        else if (c == L'}')
        {
            --depth;
            if (depth == 0) { ++pos; break; }
        }
        ++pos;
    }
    return s.substr(start, pos - start);
}

} // namespace

std::wstring GitHub::tokenFilePath()
{
    wchar_t buf[MAX_PATH] = {};
    if (::SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf) != S_OK)
    {
        return L"github_token.txt";
    }
    std::wstring dir = std::wstring(buf) + L"\\KShell";
    ::CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\github_token.txt";
}

std::wstring GitHub::loadToken()
{
    HANDLE h = ::CreateFileW(tokenFilePath().c_str(), GENERIC_READ, FILE_SHARE_READ,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    DWORD size = ::GetFileSize(h, nullptr);
    std::wstring out;
    if (size > 0 && size < 4096)
    {
        std::string bytes((size_t)size, '\0');
        DWORD read = 0;
        ::ReadFile(h, bytes.data(), size, &read, nullptr);
        int wlen = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)read, nullptr, 0);
        if (wlen > 0)
        {
            out.resize((size_t)wlen);
            MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)read, &out[0], wlen);
        }
    }
    ::CloseHandle(h);
    return trim(out);
}

void GitHub::saveToken(const std::wstring& token)
{
    std::wstring path = tokenFilePath();
    std::string bytes;
    int wlen = ::WideCharToMultiByte(CP_UTF8, 0, token.c_str(), (int)token.size(),
                                     nullptr, 0, nullptr, nullptr);
    if (wlen > 0)
    {
        bytes.resize((size_t)wlen);
        WideCharToMultiByte(CP_UTF8, 0, token.c_str(), (int)token.size(),
                            bytes.data(), wlen, nullptr, nullptr);
    }
    bytes.push_back('\n');
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                             nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        ::WriteFile(h, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
        ::CloseHandle(h);
    }
}

std::wstring GitHub::request(const wchar_t* method, const std::wstring& path,
                             const std::wstring& body, int& httpStatus)
{
    httpStatus = 0;
    lastError_.clear();
    if (token_.empty())
    {
        lastError_ = L"GitHub token is not set";
        return L"";
    }

    HINTERNET session = ::WinHttpOpen(L"KShell/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
    {
        lastError_ = L"WinHttpOpen failed";
        return L"";
    }
    HINTERNET connect = ::WinHttpConnect(session, L"api.github.com",
                                         INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect)
    {
        ::WinHttpCloseHandle(session);
        lastError_ = L"WinHttpConnect failed";
        return L"";
    }
    HINTERNET requestHandle = ::WinHttpOpenRequest(
        connect, method, path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!requestHandle)
    {
        ::WinHttpCloseHandle(connect);
        ::WinHttpCloseHandle(session);
        lastError_ = L"WinHttpOpenRequest failed";
        return L"";
    }

    std::wstring headers = L"Authorization: Bearer " + token_ + L"\r\n"
                           L"User-Agent: KShell/1.0\r\n"
                           L"Accept: application/vnd.github+json\r\n"
                           L"X-GitHub-Api-Version: 2022-11-28";
    if (!body.empty())
    {
        headers += L"\r\nContent-Type: application/json";
    }

    size_t bodyLen = (wcslen(method) ? 1 : 0);
    (void)bodyLen;

    LPVOID bodyPtr = body.empty() ? nullptr : (LPVOID)body.c_str();
    DWORD bodySize = body.empty() ? 0 : (DWORD)(body.size() * sizeof(wchar_t));
    // WinHTTP expects UTF-8 bytes for protocol; convert the wstring body.
    std::string utf8Body;
    if (!body.empty())
    {
        int wlen = ::WideCharToMultiByte(CP_UTF8, 0, body.c_str(), (int)body.size(),
                                         nullptr, 0, nullptr, nullptr);
        if (wlen > 0)
        {
            utf8Body.resize((size_t)wlen);
            WideCharToMultiByte(CP_UTF8, 0, body.c_str(), (int)body.size(),
                                utf8Body.data(), wlen, nullptr, nullptr);
        }
        bodyPtr = utf8Body.empty() ? nullptr : (LPVOID)utf8Body.data();
        bodySize = (DWORD)utf8Body.size();
    }

    BOOL sent = ::WinHttpSendRequest(requestHandle, headers.c_str(),
                                     (DWORD)headers.size(), bodyPtr, bodySize,
                                     bodySize, 0);
    if (!sent)
    {
        lastError_ = L"WinHttpSendRequest failed";
        ::WinHttpCloseHandle(requestHandle);
        ::WinHttpCloseHandle(connect);
        ::WinHttpCloseHandle(session);
        return L"";
    }
    if (!::WinHttpReceiveResponse(requestHandle, nullptr))
    {
        lastError_ = L"WinHttpReceiveResponse failed";
        ::WinHttpCloseHandle(requestHandle);
        ::WinHttpCloseHandle(connect);
        ::WinHttpCloseHandle(session);
        return L"";
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    ::WinHttpQueryHeaders(requestHandle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                          WINHTTP_NO_HEADER_INDEX);
    httpStatus = (int)statusCode;

    std::string bytes;
    DWORD available = 0;
    while (::WinHttpQueryDataAvailable(requestHandle, &available) && available > 0)
    {
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (::WinHttpReadData(requestHandle, chunk.data(), available, &read))
        {
            bytes.append(chunk, 0, read);
        }
    }

    ::WinHttpCloseHandle(requestHandle);
    ::WinHttpCloseHandle(connect);
    ::WinHttpCloseHandle(session);

    std::wstring out;
    int wlen = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(),
                                     nullptr, 0);
    if (wlen > 0)
    {
        out.resize((size_t)wlen);
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(),
                            &out[0], wlen);
    }

    if (httpStatus >= 200 && httpStatus < 300)
    {
        return out;
    }
    // Error payload: {"message": "..."}
    std::wstring msg = jsonStr(out, L"message");
    if (msg.empty())
    {
        msg = L"HTTP " + std::to_wstring(httpStatus);
        if (!out.empty() && out.size() < 200) msg += L": " + out;
    }
    lastError_ = msg;
    return L"";
}

GitHubUser GitHub::user()
{
    GitHubUser u;
    int status = 0;
    std::wstring json = request(L"GET", L"/user", L"", status);
    if (status >= 200 && status < 300)
    {
        u.login = jsonStr(json, L"login");
        u.name = jsonStr(json, L"name");
        u.avatarUrl = jsonStr(json, L"avatar_url");
    }
    return u;
}

std::vector<GitHubRepo> GitHub::repos()
{
    std::vector<GitHubRepo> out;
    int status = 0;
    std::wstring json = request(L"GET",
        L"/user/repos?per_page=100&sort=updated", L"", status);
    if (status < 200 || status >= 300) return out;
    auto objs = jsonObjects(json);
    for (auto& o : objs)
    {
        GitHubRepo r;
        r.fullName = jsonStr(o, L"full_name");
        r.isPrivate = jsonBool(o, L"private");
        r.defaultBranch = jsonStr(o, L"default_branch");
        r.htmlUrl = jsonStr(o, L"html_url");
        r.description = jsonStr(o, L"description");
        r.updatedAt = jsonStr(o, L"updated_at");
        r.stars = (int)jsonInt(o, L"stargazers_count");
        r.openIssues = (int)jsonInt(o, L"open_issues_count");
        if (!r.fullName.empty()) out.push_back(std::move(r));
    }
    return out;
}

std::vector<GitHubPR> GitHub::pulls(const std::wstring& fullName)
{
    std::vector<GitHubPR> out;
    int status = 0;
    std::wstring json = request(L"GET",
        L"/repos/" + fullName + L"/pulls?state=open&per_page=100", L"", status);
    if (status < 200 || status >= 300) return out;
    auto objs = jsonObjects(json);
    for (auto& o : objs)
    {
        GitHubPR pr;
        pr.number = std::to_wstring(jsonInt(o, L"number"));
        pr.title = jsonStr(o, L"title");
        pr.author = jsonStr(jsonObject(o, L"user"), L"login");
        std::wstring st = jsonStr(o, L"state");
        pr.state = (st == L"closed") ? PRState::Closed : PRState::Open;
        pr.headRef = jsonStr(jsonObject(o, L"head"), L"ref");
        pr.baseRef = jsonStr(jsonObject(o, L"base"), L"ref");
        pr.htmlUrl = jsonStr(o, L"html_url");
        pr.updatedAt = jsonStr(o, L"updated_at");
        pr.isDraft = jsonBool(o, L"draft");
        if (pr.title.empty()) continue;
        out.push_back(std::move(pr));
    }
    return out;
}

std::vector<GitHubIssue> GitHub::issues(const std::wstring& fullName, bool prs)
{
    std::vector<GitHubIssue> out;
    int status = 0;
    std::wstring json = request(L"GET",
        L"/repos/" + fullName + L"/issues?state=open&per_page=100", L"", status);
    if (status < 200 || status >= 300) return out;
    auto objs = jsonObjects(json);
    for (auto& o : objs)
    {
        bool isPR = keyExists(o, L"pull_request");
        if (isPR != prs) continue;
        GitHubIssue it;
        it.number = std::to_wstring(jsonInt(o, L"number"));
        it.title = jsonStr(o, L"title");
        it.author = jsonStr(jsonObject(o, L"user"), L"login");
        it.htmlUrl = jsonStr(o, L"html_url");
        it.updatedAt = jsonStr(o, L"updated_at");
        it.isPR = isPR;
        if (it.title.empty()) continue;
        out.push_back(std::move(it));
    }
    return out;
}

// JSON-escape a string for a request body.
std::wstring jsonEscape(const std::wstring& s)
{
    std::wstring out;
    for (wchar_t c : s)
    {
        switch (c)
        {
        case L'"': out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

bool GitHub::createPR(const std::wstring& fullName, const std::wstring& base,
                      const std::wstring& head, const std::wstring& title,
                      const std::wstring& body, std::wstring& outUrl)
{
    std::wstring jsonBody = L"{\"title\":\"" + jsonEscape(title) +
                            L"\",\"head\":\"" + jsonEscape(head) +
                            L"\",\"base\":\"" + jsonEscape(base) +
                            L"\",\"body\":\"" + jsonEscape(body) + L"\"}";
    int status = 0;
    std::wstring json = request(L"POST",
        L"/repos/" + fullName + L"/pulls", jsonBody, status);
    if (status < 200 || status >= 300)
    {
        return false;
    }
    outUrl = jsonStr(json, L"html_url");
    return true;
}

bool GitHub::createIssue(const std::wstring& fullName, const std::wstring& title,
                         const std::wstring& body, std::wstring& outUrl)
{
    std::wstring jsonBody = L"{\"title\":\"" + jsonEscape(title) +
                            L"\",\"body\":\"" + jsonEscape(body) + L"\"}";
    int status = 0;
    std::wstring json = request(L"POST",
        L"/repos/" + fullName + L"/issues", jsonBody, status);
    if (status < 200 || status >= 300)
    {
        return false;
    }
    outUrl = jsonStr(json, L"html_url");
    return true;
}

} // namespace kshell::github