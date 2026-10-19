# Packages Warzone 2100 for the TouchPad as an installable .ipk.
#   pkg\webos\make-ipk.ps1 -DataDir <dir> [-OutDir <dir>]
# DataDir holds the stock game data: base.wz, mp.wz and music\ (the warzone\
# folder of an earlier release ipk will do). It goes in the app's warzone\
# folder, which pmPostInstall.script moves to /media/internal/warzone on
# install. The binary is the one build-touchpad.sh leaves in pkg\webos.
# Needs the Palm SDK (palm-package) and a JDK.
param(
    [Parameter(Mandatory = $true)][string]$DataDir,
    [string]$OutDir = $PSScriptRoot,
    [string]$Sdk = "G:\webOS\PalmSDK\Current\bin",
    [string]$JavaHome = "G:\Java\jdk17"
)
$ErrorActionPreference = "Stop"
$id = "org.wz2100.warzone2100"

if (-not (Test-Path (Join-Path $PSScriptRoot "warzone2100"))) { throw "pkg\webos\warzone2100 is missing; run build-touchpad.sh first" }
foreach ($f in "base.wz", "mp.wz", "music") {
    if (-not (Test-Path (Join-Path $DataDir $f))) { throw "$DataDir has no $f" }
}

$stage = Join-Path ([IO.Path]::GetTempPath()) "wz-ipk-stage"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
$app = Join-Path $stage $id
$data = Join-Path $app "warzone"
New-Item -ItemType Directory -Force $data | Out-Null

Copy-Item (Join-Path $PSScriptRoot "appinfo.json") $app
Copy-Item (Join-Path $PSScriptRoot "icon.png") $app	# 64x64, the launcher's size
Copy-Item (Join-Path $PSScriptRoot "pmPostInstall.script") $app
Copy-Item (Join-Path $PSScriptRoot "warzone2100") $app
Copy-Item (Join-Path $DataDir "base.wz"), (Join-Path $DataDir "mp.wz") $data
Copy-Item -Recurse (Join-Path $DataDir "music") $data

$env:JAVA_HOME = $JavaHome
$env:Path = "$JavaHome\bin;$Sdk;" + $env:Path
& cmd /c "palm-package.bat -o `"$OutDir`" `"$app`""
if ($LASTEXITCODE -ne 0) { throw "palm-package failed" }
Remove-Item -Recurse -Force $stage

# the post-install script also goes in the control archive, as in the 2.3.0 release
$ipk = Get-ChildItem (Join-Path $OutDir "${id}_*_all.ipk") | Sort-Object LastWriteTime | Select-Object -Last 1
$toWsl = { param($p) (& wsl -e wslpath -a ($p -replace '\\', '/')).Trim() }
& wsl -e bash -c "tr -d '\r' < '$(& $toWsl (Join-Path $PSScriptRoot 'add-postinst.sh'))' | bash -s '$(& $toWsl $ipk.FullName)' '$(& $toWsl (Join-Path $PSScriptRoot 'pmPostInstall.script'))'"
if ($LASTEXITCODE -ne 0) { throw "adding postinst failed" }
Get-ChildItem (Join-Path $OutDir "${id}_*_all.ipk") | Format-Table Name, Length
