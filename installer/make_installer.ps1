# MakeKShellInstaller.ps1
#
# Assembles a single-file, self-extracting KShell installer:
#
#   [ installer_stub.exe ][ KShell.exe payload ][ payload length (uint64 LE) ][ magic "KSPK1" ]
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File make_installer.ps1 `
#       -SelfExe build-installer\installer_stub.exe `
#       -Payload build\KShell.exe `
#       -OutDir dist
#
# Produces: <OutDir>\KShell-Installer.exe

param(
    [Parameter(Mandatory = $true)][string]$SelfExe,
    [Parameter(Mandatory = $true)][string]$Payload,
    [Parameter(Mandatory = $true)][string]$OutDir
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $SelfExe)) { throw "Self-extractor not found: $SelfExe" }
if (-not (Test-Path -LiteralPath $Payload)) { throw "KShell.exe payload not found: $Payload" }

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$stubBytes    = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $SelfExe))
$payloadBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Payload))

# Footer: payload length as uint64 little-endian, followed by the 5-byte magic.
$lengthBytes = [BitConverter]::GetBytes([uint64]$payloadBytes.Length)
$magic       = [System.Text.Encoding]::ASCII.GetBytes('KSPK1')

$outPath = Join-Path $OutDir 'KShell-Installer.exe'
$outStream = [System.IO.File]::Create($outPath)
try {
    $outStream.Write($stubBytes, 0, $stubBytes.Length)
    $outStream.Write($payloadBytes, 0, $payloadBytes.Length)
    $outStream.Write($lengthBytes, 0, $lengthBytes.Length)
    $outStream.Write($magic, 0, $magic.Length)
} finally {
    $outStream.Dispose()
}

$info = Get-Item -LiteralPath $outPath
Write-Host ""
Write-Host "Created:   $($info.FullName)"
Write-Host "Size:      $('{0:N0}' -f $info.Length) bytes ($('{0:N2}' -f ($info.Length/1MB)) MB)"
Write-Host "Stub:      $SelfExe"
Write-Host "Payload:   $Payload ($([math]::Round($payloadBytes.Length/1MB, 2)) MB)"
Write-Host "PayloadOK: $($payloadBytes.Length -gt 0)"
