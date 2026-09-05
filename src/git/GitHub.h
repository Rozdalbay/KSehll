#pragma once

#include <string>
#include <vector>

namespace kshell::github
{

struct GitHubUser
{
    std::wstring login;
    std::wstring name;
    std::wstring avatarUrl;
};

struct GitHubRepo
{
    std::wstring fullName;
    bool         isPrivate = false;
    std::wstring defaultBranch;
    std::wstring htmlUrl;
    std::wstring description;
    std::wstring updatedAt;
    int          stars = 0;
    int          openIssues = 0;
};

enum class PRState { Open, Closed, Merged };

struct GitHubPR
{
    std::wstring number;
    std::wstring title;
    PRState      state = PRState::Open;
    std::wstring author;
    std::wstring headRef;
    std::wstring baseRef;
    std::wstring htmlUrl;
    std::wstring updatedAt;
    int          commits = 0;
    int          additions = 0;
    int          deletions = 0;
    bool         isDraft = false;
};

struct GitHubIssue
{
    std::wstring number;
    std::wstring title;
    bool         open = true;
    bool         isPR = false;
    std::wstring author;
    std::wstring htmlUrl;
    std::wstring updatedAt;
};

class GitHub
{
public:
    // Token storage: %APPDATA%\KShell\github_token.txt
    static std::wstring tokenFilePath();
    static std::wstring loadToken();
    static void         saveToken(const std::wstring& token);

    void setToken(const std::wstring& token) { token_ = token; }

    // Any stale network warnings get reset on each call.
    GitHubUser user();
    // List the authenticated user's repositories.
    std::vector<GitHubRepo> repos();
    // List pull requests for a repository ("owner/repo").
    std::vector<GitHubPR> pulls(const std::wstring& fullName);
    // List issues (PRs excluded unless prs=true).
    std::vector<GitHubIssue> issues(const std::wstring& fullName, bool prs);
    // Creates a PR. Returns created HTML url on success.
    bool createPR(const std::wstring& fullName, const std::wstring& base,
                  const std::wstring& head, const std::wstring& title,
                  const std::wstring& body, std::wstring& outUrl);
    bool createIssue(const std::wstring& fullName, const std::wstring& title,
                     const std::wstring& body, std::wstring& outUrl);

    std::wstring lastError() const { return lastError_; }

private:
    // Raw request: GET/POST; returns API JSON body or "" on error.
    std::wstring request(const wchar_t* method, const std::wstring& path,
                         const std::wstring& body, int& httpStatus);

    std::wstring token_;
    std::wstring lastError_;
};

} // namespace kshell::github