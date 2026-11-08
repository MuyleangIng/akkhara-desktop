# Add (or with -Remove: remove) the Akkhara keyboard in the current user's language list,
# under Khmer. Same as Settings → Time & language → Language & region → Khmer → Add a keyboard.
param(
    [Parameter(Mandatory = $true)][string]$Tip,
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$list = Get-WinUserLanguageList
$khmer = $list | Where-Object { $_.LanguageTag -like 'km*' } | Select-Object -First 1

if ($Remove) {
    if ($khmer -and $khmer.InputMethodTips.Contains($Tip)) {
        [void]$khmer.InputMethodTips.Remove($Tip)
        if ($khmer.InputMethodTips.Count -eq 0) { [void]$list.Remove($khmer) }
        Set-WinUserLanguageList $list -Force
    }
    exit 0
}

if (-not $khmer) {
    $list.Add('km-KH')
    $khmer = $list | Where-Object { $_.LanguageTag -like 'km*' } | Select-Object -First 1
    # Akkhara becomes the only Khmer keyboard (the NiDA Khmer layout can be added in Settings)
    $khmer.InputMethodTips.Clear()
}
if (-not $khmer.InputMethodTips.Contains($Tip)) { $khmer.InputMethodTips.Add($Tip) }
Set-WinUserLanguageList $list -Force
Write-Output "Akkhara added: $($khmer.LanguageTag) -> $Tip"
