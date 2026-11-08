# Akkhara updater: compares this PC's version with the newest GitHub release and offers to install it.
# The keyboard itself never goes online; only this small script does, when it runs:
#   - Start menu → Akkhara → "Check for updates"           (shows "you have the latest" too)
#   - once a week, if chosen in Setup (scheduled task "Akkhara update check", -Quiet: only asks when
#     there IS a new version)
# Installing the new Setup over the old one keeps the folder and needs no restart.
param([switch]$Quiet)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$repo = 'MuyleangIng/akkhara-desktop'
$appId = '{083E06E0-49D0-45DB-BC58-17EFD302DAD8}_is1'

function Say([string]$text, [string]$buttons = 'OK') {
    [System.Windows.Forms.MessageBox]::Show($text, 'Akkhara', $buttons, 'Information')
}

# installed version (written by Setup into the Windows "installed apps" list)
$installed = $null
foreach ($root in 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall',
                  'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall') {
    $k = Join-Path $root $appId
    if (Test-Path $k) { $installed = (Get-ItemProperty $k).DisplayVersion; break }
}

try {
    $rel = Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest" -Headers @{ 'User-Agent' = 'Akkhara-updater' } -TimeoutSec 20
} catch {
    if (-not $Quiet) { Say "Could not reach GitHub to check for updates.`n`n$($_.Exception.Message)" }
    exit 1
}
$latest = $rel.tag_name.TrimStart('v')
$asset = $rel.assets | Where-Object { $_.name -like 'Akkhara-Keyboard-Setup-*.exe' } | Select-Object -First 1

$newer = $true
try { if ($installed) { $newer = [version]$latest -gt [version]$installed } } catch { $newer = $latest -ne $installed }

if (-not $newer -or -not $asset) {
    if (-not $Quiet) { Say "You have the latest Akkhara ($installed)." }
    exit 0
}

$answer = Say "Akkhara $latest is available (you have $installed).`n`nInstall it now? No restart is needed." 'YesNo'
if ($answer -ne 'Yes') { exit 0 }

$file = Join-Path $env:TEMP $asset.name
Invoke-WebRequest $asset.browser_download_url -OutFile $file -UseBasicParsing
# same AppId → Setup updates the existing install in its folder; Windows asks for admin (UAC)
Start-Process $file -ArgumentList '/SILENT', '/SUPPRESSMSGBOXES', '/NORESTART'
