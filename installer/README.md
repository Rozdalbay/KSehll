# KShell Installer

A single-file, self-contained installer for **KShell**. You can copy
`KShell-Installer.exe` to any Windows 10/11 x64 machine and run it — it installs
everything with **no admin rights** and **no external dependencies** (no NSIS,
Inno Setup, or 7-Zip required).

## How to use

Just run the file:

```text
KShell-Installer.exe
```

It will show a short prompt for the install location (default is
`%LOCALAPPDATA%\Programs\KShell`), install `KShell.exe`, create Start-menu and
desktop shortcuts, and prepare the `%APPDATA%\KShell` config/log folders.

### Options

```text
KShell-Installer.exe                Interactive install
KShell-Installer.exe /S             Silent install to the default location
KShell-Installer.exe /S /D=DIR      Silent install to a specific folder
KShell-Installer.exe /S /PATH       Silent install and add the folder to user PATH
KShell-Installer.exe /?             Show usage
```

Examples:

```text
KShell-Installer.exe /S
KShell-Installer.exe /S /D=C:\Tools\KShell /PATH
```

## What it installs

| Item                    | Location                                   |
|-------------------------|--------------------------------------------|
| KShell.exe              | Install directory (e.g. `%LOCALAPPDATA%\Programs\KShell`) |
| Start-menu shortcut     | `Start Menu\Programs\KShell\KShell.lnk`   |
| Desktop shortcut        | Desktop `KShell.lnk`                        |
| Config / logs folders   | `%APPDATA%\KShell` (config + logs)         |
| (optional) user PATH    | Adds the install directory to the user PATH |

The installed `KShell.exe` is statically linked, so it runs on any modern
Windows without bundling runtime DLLs.

## How the single-file format works

The installer is a self-extracting executable:

```text
[ KShell.exe payload ][ payload length (uint64 LE) ][ magic "KSPK1" ]
```

- The payload (the real `KShell.exe`) is appended to the installer stub.
- A small footer records the payload length and a magic marker.
- On launch the installer reads its own file, locates the footer, extracts the
  payload byte-for-byte, and writes it to the chosen install directory.

## How to rebuild the installer

If you modify KShell and want a fresh installer:

```powershell
# 1. Build KShell.exe (from the repo root)
cmake -S . -B build
cmake --build build

# 2. Build the installer stub
cmake -S installer -B installer\build-installer `
    -G "MinGW Makefiles" -DKSHELL_EXE=C:\shell\build\KShell.exe
cmake --build installer\build-installer

# 3. Assemble the final single-file installer
powershell -ExecutionPolicy Bypass -File installer\make_installer.ps1 `
    -SelfExe installer\build-installer\installer_stub.exe `
    -Payload build\KShell.exe `
    -OutDir dist
```

The finished installer is written to `dist\KShell-Installer.exe`.

## Files

```text
installer/
├── main.cpp              Installer source (payload extraction, install, shortcuts, PATH)
├── CMakeLists.txt        Build config for the installer stub
├── make_installer.ps1    Assembles the final self-extracting single-file installer
└── README.md             This file
```
