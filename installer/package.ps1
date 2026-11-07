# package.ps1 - build the Inno Setup installer into dist/
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$iscc = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup 6 (ISCC.exe) not found" }

if (-not (Test-Path "$root\build\release\EchoesOfTheAbyss.exe")) {
    throw "release build missing; run 'make release' first"
}

New-Item -ItemType Directory -Path "$root\dist" -Force | Out-Null
& $iscc "$root\installer\setup.iss"
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit $LASTEXITCODE" }

Get-ChildItem "$root\dist" -Filter *.exe | Select-Object Name, Length
Write-Output "package ok"
