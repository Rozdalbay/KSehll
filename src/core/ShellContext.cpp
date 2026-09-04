#include "core/ShellContext.h"

#include "utils/StringUtils.h"
#include "utils/PathUtils.h"

namespace kshell
{

ShellContext::ShellContext()
    : sink_(&consoleSink_)
{
}

ShellContext::~ShellContext() = default;

bool ShellContext::initialize()
{
    if (initialized_)
    {
        return true;
    }
    if (!console_.initialize())
    {
        return false;
    }

    config_.load();

    auto home = pathutils::getHomeDirectory();
    environment_.set(L"HOME", home);
    environment_.set(L"USER", environment_.getUser());
    environment_.set(L"PWD", currentDirectory());

    logger_.open(config_.getLogFilePath());
    logger_.info(L"KShell " + std::wstring(KSHELL_VERSION) + L" started");

    history_.loadFromFile(config_.getHistoryFilePath());
    history_.setMaxSize(config_.historySize);

    resetPromptDefaults();
    refreshExecutor();

    initialized_ = true;
    return true;
}

std::wstring ShellContext::currentDirectory() const
{
    return pathutils::getCurrentDirectory();
}

bool ShellContext::setCurrentDirectory(const std::wstring& path)
{
    const std::wstring expanded = pathutils::expandPath(path);
    if (!pathutils::pathExists(expanded) || !pathutils::isDirectory(expanded))
    {
        return false;
    }
    if (!::SetCurrentDirectoryW(expanded.c_str()))
    {
        return false;
    }
    environment_.set(L"PWD", currentDirectory());
    return true;
}

std::wstring ShellContext::promptText() const
{
    const std::wstring user = environment_.getUser();
    const std::wstring host = environment_.getHostname();
    const std::wstring cwd = currentDirectory();

    std::wstring cwdDisplay = cwd;
    const std::wstring home = pathutils::getHomeDirectory();
    if (cwdDisplay.size() >= home.size() &&
        stringutils::equalsIgnoreCase(cwdDisplay.substr(0, home.size()), home))
    {
        cwdDisplay = L"~" + cwdDisplay.substr(home.size());
    }

    return user + L"@" + host + L" " + cwdDisplay + L"> ";
}

void ShellContext::resetPromptDefaults()
{
    std::wstring currentPrompt = config_.prompt;
    if (currentPrompt.empty())
    {
        config_.prompt = promptText();
    }
}

void ShellContext::printPrompt()
{
    sink_->printPrompt(promptText(), config_.colors);
}

void ShellContext::printError(const std::wstring& text)
{
    logger_.error(text);
    sink_->printError(text);
}

void ShellContext::printSuccess(const std::wstring& text)
{
    sink_->printSuccess(text);
}

void ShellContext::printOutput(const std::wstring& text)
{
    sink_->print(text);
    sink_->print(L"\n");
}

void ShellContext::refreshExecutor()
{
    ExecutionContext ctx;
    ctx.workingDirectory = currentDirectory();
    ctx.environmentBlock = buildEnvironmentBlock();
    ctx.path = environment_.getPath();
    ctx.prompt = promptText();
    executor_ = std::make_unique<CommandExecutor>(std::move(ctx));
}

std::wstring ShellContext::buildEnvironmentBlock() const
{
    const auto all = environment_.getAll();
    std::wstring block;
    for (const auto& [name, value] : all)
    {
        block += name + L"=" + value + L"\0";
    }
    block += L"\0";
    return block;
}

} // namespace kshell