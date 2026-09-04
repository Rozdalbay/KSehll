#pragma once

#ifndef KSHELL_ENVIRONMENT_H
#define KSHELL_ENVIRONMENT_H

#include <string>
#include <map>
#include <set>
#include <optional>

namespace kshell
{

class Environment
{
public:
    Environment();

    std::optional<std::wstring> get(const std::wstring& name) const;

    void set(const std::wstring& name, const std::wstring& value);

    void unset(const std::wstring& name);

    std::map<std::wstring, std::wstring> getAll() const;

    // Names of variables explicitly set through set()/unset() by the user.
    const std::set<std::wstring>& userSetNames() const { return userSet_; }
    bool isUserSet(const std::wstring& name) const { return userSet_.count(name) > 0; }

    std::wstring getPath() const;

    std::wstring getHome() const;

    std::wstring getUser() const;

    std::wstring getHostname() const;

    std::wstring getPwd() const;

    void refresh();

    std::wstring expand(const std::wstring& input) const;

private:
    static const std::map<std::wstring, std::wstring>& defaultVariables();
    std::map<std::wstring, std::wstring> variables_;
    std::set<std::wstring> userSet_;
};

} // namespace kshell

#endif // KSHELL_ENVIRONMENT_H
