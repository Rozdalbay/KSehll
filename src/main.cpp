#include "core/Shell.h"
#include "ui/App.h"

#include <iostream>

#include "utils/StringUtils.h"

namespace
{

bool isArg(const wchar_t* arg, const wchar_t* longName, const wchar_t* shortName)
{
    if (shortName != nullptr && kshell::stringutils::equalsIgnoreCase(arg, shortName))
    {
        return true;
    }
    return kshell::stringutils::equalsIgnoreCase(arg, longName);
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    bool legacyMode = false;
    bool tuiMode = false;

    for (int i = 1; i < argc; ++i)
    {
        if (isArg(argv[i], L"--version", L"-v"))
        {
            std::wcout << L"KShell " << KSHELL_VERSION << L"\n";
            std::wcout << L"C++20\n";
            std::wcout << L"Windows x64\n";
            return 0;
        }
        if (isArg(argv[i], L"--help", L"-h") || isArg(argv[i], L"/?", nullptr))
        {
            std::wcout << L"KShell " << KSHELL_VERSION << L"\n\n";
            std::wcout << L"Usage: KShell [options]\n\n";
            std::wcout << L"Options:\n";
            std::wcout << L"  --version, -v   Show version information\n";
            std::wcout << L"  --help, -h      Show this help message\n";
            std::wcout << L"  --legacy        Use the classic console REPL\n";
            std::wcout << L"  --tui           Force the TUI environment\n";
            std::wcout << L"\n";
            std::wcout << L"Default: TUI environment with panels, tabs, and command palette.\n";
            return 0;
        }
        if (isArg(argv[i], L"--legacy", nullptr))
        {
            legacyMode = true;
        }
        if (isArg(argv[i], L"--tui", nullptr))
        {
            tuiMode = true;
        }
    }

    if (legacyMode)
    {
        kshell::Shell shell;
        if (!shell.initialize())
        {
            std::wcerr << L"Failed to initialize KShell\n";
            return 1;
        }
        return shell.run();
    }

    // Default: TUI mode.
    kshell::ui::App app;
    if (!app.initialize(argc, argv))
    {
        // Fallback to classic shell if TUI fails to initialize.
        kshell::Shell shell;
        if (!shell.initialize())
        {
            std::wcerr << L"Failed to initialize KShell\n";
            return 1;
        }
        return shell.run();
    }

    int exitCode = app.run();
    app.shutdown();
    return exitCode;
}