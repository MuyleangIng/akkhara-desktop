# Test helper: keep the keyboard DLLs loaded (like an open app does) for -Seconds.
param([Parameter(Mandatory = $true)][string]$Dir, [int]$Seconds = 120)
Add-Type -Namespace W -Name K -MemberDefinition '[DllImport("kernel32", CharSet = CharSet.Unicode)] public static extern System.IntPtr LoadLibrary(string p);'
foreach ($f in "akkhara-ort.dll", "AkkharaIME.dll") { "load $f -> $([W.K]::LoadLibrary("$Dir\x64\$f"))" }
Start-Sleep $Seconds
