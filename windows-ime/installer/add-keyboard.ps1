# Add (or with -Remove: remove) the Akkhara keyboard in the current user's language list,
# under Khmer. Same as Settings → Time & language → Language & region → Khmer → Add a keyboard.
# Exit code 0 = done and checked; 1 = it did not work (Setup then shows the manual steps).
param(
    [Parameter(Mandatory = $true)][string]$Tip,
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
try {
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

    # check that Windows really has it now
    $check = Get-WinUserLanguageList | Where-Object { $_.InputMethodTips -contains $Tip }
    if (-not $check) { Write-Output "Akkhara is NOT in the keyboard list"; exit 1 }
    Write-Output "Akkhara added: $($check.LanguageTag) -> $Tip"
    exit 0
} catch {
    Write-Output "failed: $_"
    exit 1
}
