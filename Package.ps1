param([Parameter(Mandatory)][string]$Tag)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$version = [regex]::Match((Get-Content CMakeLists.txt -Raw), 'project\(SimpleMods VERSION ([0-9.]+)').Groups[1].Value
if ($Tag -ne "v$version") { throw 'The tag must match the version in CMakeLists.txt.' }
$stage = Join-Path $PSScriptRoot ("dist/release-" + $version)
if (Test-Path -LiteralPath $stage) { throw 'The release staging folder already exists. Use a clean checkout.' }
New-Item -ItemType Directory -Path $stage | Out-Null
foreach ($name in @('Install.cmd','Install.ps1','Uninstall.cmd','README.md','README.ru.md')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $stage
}
foreach ($name in @('payload','SDK','licenses')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $stage -Recurse
}
Copy-Item -LiteralPath 'build/Release/xinput1_3.dll' -Destination (Join-Path $stage 'payload/xinput1_3.dll') -Force
$archive = Join-Path $PSScriptRoot ("dist/Simple-Mods-" + $version + ".zip")
Compress-Archive -Path ($stage + '/*') -DestinationPath $archive
Write-Output $archive

