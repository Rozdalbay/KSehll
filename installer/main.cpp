//
// KShell Installer
//
// A self-contained, single-file installer for KShell.
//
// How it works:
//   The real KShell.exe binary is appended to this installer EXE, followed by a
//   small footer:
//
//       [ KShell.exe payload bytes ... ][ PAYLOAD_LENGTH (uint64 LE) ][ MAGIC "KSPK1" ]
//
//   On launch the installer:
//     - locates its own file via GetModuleFileNameW,
//     - reads the footer to find and extract the embedded payload,
//     - creates the install directory and writes KShell.exe there,
//     - creates Start-menu and desktop shortcuts,
//     - creates the %APPDATA%\KShell config/log directories,
//     - optionally appends the install dir to the user PATH,
//     - (silent mode) installs without prompting.
//
// Usage:
//   KShell-Installer.exe                  interactive install
//   KShell-Installer.exe /S               silent install to the default location
//   KShell-Installer.exe /S /D=C:\App     silent install to a specific folder
//
// No admin rights are required: KShell installs per-user by default. No external
// dependencies (NSIS / Inno / 7-Zip) are needed.
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <shobjidl.h>

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cwctype>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iostream>

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

namespace fs = std::filesystem;

// Footer magic: 5 bytes. A version byte is included for future format changes.
static const char kMagic[5] = {'K', 'S', 'P', 'K', '1'};
static constexpr int kFooterSize = 8 + 5; // uint64 LE payload length + magic

// ---------------------------------------------------------------------------
// Payload extraction helpers
// ---------------------------------------------------------------------------

struct Footer
{
    uint64_t payloadLength = 0;
    bool valid = false;
};

// Read the footer located at the very end of the given file.
static Footer readFooter(const fs::path& selfPath)
{
    Footer footer;
    std::ifstream stream(selfPath, std::ios::binary);
    if (!stream.is_open())
    {
        return footer;
    }

    stream.seekg(0, std::ios::end);
    const std::streamoff fileSize = stream.tellg();
    if (fileSize < kFooterSize)
    {
        return footer;
    }

    std::vector<char> tail(static_cast<size_t>(kFooterSize));
    stream.seekg(-kFooterSize, std::ios::end);
    stream.read(tail.data(), kFooterSize);
    if (stream.gcount() != kFooterSize)
    {
        return footer;
    }

    if (std::memcmp(tail.data() + 8, kMagic, 5) != 0)
    {
        // No payload appended — this is the raw installer with no embedded KShell.
        return footer;
    }

    uint64_t payloadLength = 0;
    std::memcpy(&payloadLength, tail.data(), 8);

    if (payloadLength == 0 || payloadLength + kFooterSize > static_cast<uint64_t>(fileSize))
    {
        return footer;
    }

    footer.payloadLength = payloadLength;
    footer.valid = true;
    return footer;
}

// Extract the embedded payload bytes from this installer file. The payload
// occupies the last `payloadLength` bytes located immediately before the footer.
static bool extractPayload(const fs::path& selfPath, uint64_t payloadLength,
                           std::vector<char>& outBytes)
{
    std::ifstream stream(selfPath, std::ios::binary);
    if (!stream.is_open())
    {
        return false;
    }

    stream.seekg(0, std::ios::end);
    const std::streamoff fileSize = stream.tellg();
    if (fileSize <= 0 || static_cast<uint64_t>(fileSize) < payloadLength + kFooterSize)
    {
        return false;
    }

    const std::streamoff payloadStart = static_cast<std::streamoff>(
        static_cast<uint64_t>(fileSize) - payloadLength - kFooterSize);

    outBytes.resize(static_cast<size_t>(payloadLength));
    stream.seekg(payloadStart, std::ios::beg);
    stream.read(outBytes.data(), static_cast<std::streamsize>(payloadLength));
    if (stream.gcount() != static_cast<std::streamsize>(payloadLength))
    {
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Windows helpers
// ---------------------------------------------------------------------------

// Resolve an environment string like %LOCALAPPDATA%.
static std::wstring expandEnv(const std::wstring& input)
{
    DWORD size = ::ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (size == 0)
    {
        return input;
    }
    std::wstring buffer(static_cast<size_t>(size - 1), L'\0');
    ::ExpandEnvironmentStringsW(input.c_str(), buffer.data(), size);
    return buffer;
}

// Create a shell shortcut (.lnk) file.
static bool createShortcut(const std::wstring& lnkPath, const std::wstring& targetExe,
                           const std::wstring& workingDir, const std::wstring& description)
{
    ::CoInitialize(nullptr);
    bool ok = false;

    IShellLinkW* shellLink = nullptr;
    if (SUCCEEDED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_IShellLinkW, reinterpret_cast<void**>(&shellLink))))
    {
        shellLink->SetPath(targetExe.c_str());
        shellLink->SetWorkingDirectory(workingDir.c_str());
        if (!description.empty())
        {
            shellLink->SetDescription(description.c_str());
        }

        IPersistFile* persist = nullptr;
        if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist))))
        {
            ok = SUCCEEDED(persist->Save(lnkPath.c_str(), TRUE));
            persist->Release();
        }
        shellLink->Release();
    }

    ::CoUninitialize();
    return ok;
}

static std::wstring getStartMenuDir()
{
    wchar_t path[MAX_PATH] = {0};
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, path)))
    {
        return std::wstring(path) + L"\\KShell";
    }
    return expandEnv(L"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\KShell");
}

static std::wstring getDesktopDir()
{
    wchar_t path[MAX_PATH] = {0};
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path)))
    {
        return std::wstring(path);
    }
    return expandEnv(L"%USERPROFILE%\\Desktop");
}

// Read the current user PATH registry value (REG_SZ / REG_EXPAND_SZ).
static std::wstring getUserPath()
{
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        return L"";
    }

    std::wstring value;
    DWORD size = 0;
    ::RegQueryValueExW(key, L"Path", nullptr, nullptr, nullptr, &size);
    if (size > 0)
    {
        std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 2, L'\0');
        DWORD type = 0;
        LSTATUS status = ::RegQueryValueExW(key, L"Path", nullptr, &type,
                                            reinterpret_cast<LPBYTE>(buffer.data()), &size);
        if (status == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ))
        {
            value = buffer.data();
        }
    }
    ::RegCloseKey(key);
    return value;
}

// Append a directory to the user PATH if not already present.
static bool addToUserPath(const std::wstring& dir)
{
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
    {
        return false;
    }

    std::wstring current = getUserPath();
    const std::wstring dirLower = [&]() { std::wstring s = dir; for (auto& c : s) c = towlower(c); return s; }();

    // Check case-insensitively whether `dir` is already a PATH entry.
    bool present = false;
    std::wstring token;
    for (wchar_t c : current)
    {
        if (c == L';')
        {
            std::wstring t = token;
            for (auto& ch : t) ch = towlower(ch);
            if (t == dirLower)
            {
                present = true;
                break;
            }
            token.clear();
        }
        else
        {
            token.push_back(c);
        }
    }
    if (!present)
    {
        std::wstring t = token;
        for (auto& ch : t) ch = towlower(ch);
        if (t == dirLower)
        {
            present = true;
        }
    }

    bool ok = true;
    if (!present && !current.empty())
    {
        current += std::wstring(1, L';');
    }
    if (!present)
    {
        current += dir;
        const std::wstring regPath = L"PATH";
        ok = ::RegSetValueExW(key, L"Path", 0, REG_EXPAND_SZ,
                              reinterpret_cast<const BYTE*>(current.c_str()),
                              static_cast<DWORD>((current.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    }

    ::RegCloseKey(key);
    if (ok)
    {
        // Notify the system that the environment changed (broadcast is optional).
        ::SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                              reinterpret_cast<LPARAM>(L"Environment"),
                              SMTO_ABORTIFHUNG, 5000, nullptr);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

struct InstallOptions
{
    bool silent = false;
    bool addPath = false;
    bool pathSpecified = false;
    std::wstring installDir;
};

static InstallOptions parseArguments(int argc, wchar_t* argv[])
{
    InstallOptions opts;

    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        if (arg == L"/S" || arg == L"-S" || arg == L"/silent" || arg == L"-silent")
        {
            opts.silent = true;
        }
        else if (arg == L"/PATH" || arg == L"-PATH" || arg == L"/addpath")
        {
            opts.addPath = true;
        }
        else if (arg.size() > 2 && (arg[0] == L'/') && (arg[1] == L'D' || arg[1] == L'd') &&
                 arg[2] == L'=')
        {
            opts.installDir = arg.substr(3);
            opts.pathSpecified = true;
        }
        else if (arg.size() > 3 && arg.substr(0, 3) == L"/D=")
        {
            opts.installDir = arg.substr(3);
            opts.pathSpecified = true;
        }
    }
    return opts;
}

// ---------------------------------------------------------------------------
// Main install routine
// ---------------------------------------------------------------------------

static int printUsage()
{
    fwprintf(stdout,
        L"\n"
        L"KShell Installer\n"
        L"----------------\n"
        L"Usage:\n"
        L"  KShell-Installer.exe                Interactive install\n"
        L"  KShell-Installer.exe /S             Silent install (default location)\n"
        L"  KShell-Installer.exe /S /D=DIR      Silent install to DIR\n"
        L"  KShell-Installer.exe /PATH          Also add install dir to user PATH\n"
        L"\n"
        L"Default install location: %%LOCALAPPDATA%%\\Programs\\KShell\n");
    return 0;
}

static std::wstring getDefaultInstallDir()
{
    const std::wstring base = expandEnv(L"%LOCALAPPDATA%\\Programs");
    return base + L"\\KShell";
}

// Wait for a key press so the console window does not vanish instantly.
static void pauseForKey()
{
    std::wcout << L"\nPress Enter to close...";
    std::wstring dummy;
    std::getline(std::wcin, dummy);
}

static int runInstaller(const InstallOptions& opts)
{
    // 1. Locate self and read footer.
    wchar_t selfPath[MAX_PATH] = {0};
    if (::GetModuleFileNameW(nullptr, selfPath, MAX_PATH) == 0)
    {
        fwprintf(stderr, L"Error: could not locate installer executable.\n");
        return 1;
    }
    const fs::path selfFile(selfPath);
    const Footer footer = readFooter(selfFile);

    fwprintf(stdout, L"\nKShell Installer\n");
    fwprintf(stdout, L"-----------------\n\n");

    if (!footer.valid)
    {
        fwprintf(stderr,
                 L"Error: no embedded KShell payload found in this installer.\n"
                 L"This looks like the raw installer stub (installer_stub.exe), not the\n"
                 L"final package. Use the assembled KShell-Installer.exe instead, which\n"
                 L"contains the actual KShell.exe built in.\n");
        pauseForKey();
        return 1;
    }

    std::vector<char> payload;
    if (!extractPayload(selfFile, footer.payloadLength, payload) || payload.empty())
    {
        fwprintf(stderr, L"Error: failed to read the embedded KShell payload.\n");
        pauseForKey();
        return 1;
    }

    const size_t payloadSize = payload.size();
    wprintf(L"Embedded KShell payload: %zu KB\n", payloadSize / 1024);

    // 2. Resolve install directory.
    std::wstring installDir = opts.installDir.empty() ? getDefaultInstallDir() : expandEnv(opts.installDir);
    if (installDir.empty())
    {
        installDir = getDefaultInstallDir();
    }

    if (!opts.silent)
    {
        wprintf(L"Install directory [%ls]: ", installDir.c_str());
        std::wstring entered;
        std::getline(std::wcin, entered);
        if (!entered.empty())
        {
            installDir = expandEnv(entered);
        }
    }

    // 3. Create install directory and write KShell.exe.
    std::error_code ec;
    fs::create_directories(fs::path(installDir), ec);
    if (ec)
    {
        fwprintf(stderr, L"Error: could not create install directory:\n%ls\n", installDir.c_str());
        pauseForKey();
        return 1;
    }

    const fs::path targetExe = fs::path(installDir) / L"KShell.exe";
    {
        std::ofstream out(targetExe, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            fwprintf(stderr, L"Error: could not write:\n%ls\n", targetExe.c_str());
            pauseForKey();
            return 1;
        }
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        out.flush();
        if (!out)
        {
            fwprintf(stderr, L"Error: failed while writing KShell.exe.\n");
            pauseForKey();
            return 1;
        }
    }

    // 4. Create config / log directories.
    std::wstring appData = expandEnv(L"%APPDATA%\\KShell");
    fs::create_directories(fs::path(appData) / L"logs", ec);

    // 5. Create Start-menu and desktop shortcuts.
    const std::wstring menuDir = getStartMenuDir();
    fs::create_directories(fs::path(menuDir), ec);
    createShortcut(menuDir + L"\\KShell.lnk", targetExe.wstring(), installDir, L"KShell - native Windows command shell");

    const std::wstring desktopDir = getDesktopDir();
    if (!desktopDir.empty())
    {
        createShortcut(desktopDir + L"\\KShell.lnk", targetExe.wstring(), installDir, L"KShell");
    }

    // 6. Optionally add to PATH.
    if (opts.addPath)
    {
        if (addToUserPath(installDir))
        {
            wprintf(L"Added %ls to user PATH.\n", installDir.c_str());
        }
        else
        {
            fwprintf(stderr, L"Warning: could not update user PATH.\n");
        }
    }

    wprintf(L"\nKShell installed successfully!\n");
    wprintf(L"  Install dir : %ls\n", installDir.c_str());
    wprintf(L"  Executable  : %ls\n", targetExe.c_str());
    wprintf(L"  Config/logs : %ls\n", appData.c_str());
    wprintf(L"\n");

    if (!opts.silent)
    {
        wprintf(L"Launch KShell now? [y/N]: ");
        std::wstring answer;
        std::getline(std::wcin, answer);
        if (!answer.empty() && (answer[0] == L'y' || answer[0] == L'Y'))
        {
            ::ShellExecuteW(nullptr, L"open", targetExe.c_str(), nullptr, installDir.c_str(), SW_SHOWNORMAL);
        }
    }

    return 0;
}

int wmain(int argc, wchar_t* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        if (arg == L"/?" || arg == L"-?" || arg == L"/h" || arg == L"-h" || arg == L"/help" || arg == L"--help")
        {
            return printUsage();
        }
    }

    const InstallOptions opts = parseArguments(argc, argv);
    return runInstaller(opts);
}
