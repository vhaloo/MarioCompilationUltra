# Install the complete portable package for the current user; preserve local game data.
$ErrorActionPreference = 'Stop'
$mcuPackage = $PSScriptRoot
$mcuTarget = Join-Path $env:LOCALAPPDATA 'Programs\MarioCompilationUltra'
if (-not (Test-Path -LiteralPath (Join-Path $mcuPackage 'MarioCompilationUltra.exe'))) {
    throw 'Extract the complete Windows package before running this installer.'
}
if (-not [string]::Equals([IO.Path]::GetFullPath($mcuPackage).TrimEnd('\'), [IO.Path]::GetFullPath($mcuTarget).TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) {
    New-Item -ItemType Directory -Path $mcuTarget -Force | Out-Null
    foreach ($mcuItem in Get-ChildItem -LiteralPath $mcuPackage) {
        Copy-Item -LiteralPath $mcuItem.FullName -Destination $mcuTarget -Recurse -Force
    }
}
$mcuShell = New-Object -ComObject WScript.Shell
$mcuShortcut = $mcuShell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'Mario Compilation Ultra.lnk'))
$mcuShortcut.TargetPath = Join-Path $mcuTarget 'MarioCompilationUltra.exe'
$mcuShortcut.WorkingDirectory = $mcuTarget
# Keep an existing explicit local-data folder when updating the desktop launcher.
# Only preserve this option; temporary test/game/report arguments are discarded.
$mcuDataArgument = ''
if ($mcuShortcut.Arguments -match '(?:^|\s)--data-dir\s+(?:"([^"]+)"|(\S+))') {
    $mcuDataFolder = if ($Matches[1]) { $Matches[1] } else { $Matches[2] }
    if ([IO.Path]::IsPathRooted($mcuDataFolder)) {
        $mcuDataArgument = ' --data-dir "' + $mcuDataFolder + '"'
    }
}
$mcuShortcut.Arguments = '--menu' + $mcuDataArgument
$mcuShortcut.IconLocation = "$mcuTarget\MarioCompilationUltra.exe,0"
$mcuShortcut.Description = 'Mario Compilation Ultra'
$mcuShortcut.Save()
Write-Output $mcuShortcut.FullName
