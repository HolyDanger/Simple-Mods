param([string]$GamePath, [switch]$Remove)
$ErrorActionPreference = 'Stop'
$russian = [Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ru'
function Text([string]$English, [string]$Russian) { if ($russian) { $Russian } else { $English } }
try {
    if (-not $GamePath) {
        foreach ($candidate in @($PSScriptRoot, (Split-Path $PSScriptRoot -Parent))) {
            if (Test-Path -LiteralPath (Join-Path $candidate 'HelloNeighbor.exe')) { $GamePath = $candidate; break }
        }
    }
    if (-not $GamePath) {
        Add-Type -AssemblyName System.Windows.Forms
        $picker = New-Object System.Windows.Forms.FolderBrowserDialog
        $picker.Description = Text 'Select the Hello Neighbor folder containing HelloNeighbor.exe' 'Выберите папку Hello Neighbor, в которой находится HelloNeighbor.exe'
        if ($picker.ShowDialog() -ne 'OK') { exit 1 }
        $GamePath = $picker.SelectedPath
        $picker.Dispose()
    }
    $gameRoot = (Resolve-Path -LiteralPath $GamePath).Path
    $bin = Join-Path $gameRoot 'HelloNeighbor\Binaries\Win64'
    $gameExe = Join-Path $bin 'HelloNeighbor-Win64-Shipping.exe'
    if (-not (Test-Path -LiteralPath $gameExe)) { throw (Text 'Hello Neighbor was not found in the selected folder.' 'В выбранной папке не найдена игра.') }
    $running = Get-Process -Name 'HelloNeighbor-Win64-Shipping' -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and [string]::Equals($_.Path, $gameExe, [StringComparison]::OrdinalIgnoreCase) }
    if ($running) { throw (Text 'Close Hello Neighbor before running the installer.' 'Сначала закройте Hello Neighbor, затем снова запустите установку.') }
    $source = Join-Path $PSScriptRoot 'payload\xinput1_3.dll'
    $target = Join-Path $bin 'xinput1_3.dll'
    $stateDir = Join-Path $bin 'SimpleMods'
    $receiptPath = Join-Path $stateDir 'install.json'
    $legacyDir = Join-Path $bin 'NeighborMods'
    if (-not (Test-Path -LiteralPath $stateDir) -and (Test-Path -LiteralPath $legacyDir)) { Copy-Item -LiteralPath $legacyDir -Destination $stateDir -Recurse }
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $receipt = if (Test-Path -LiteralPath $receiptPath) { Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json } else { $null }
    if (Test-Path -LiteralPath $target) {
        $existingHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        if ($existingHash -ne $sourceHash -and (-not $receipt -or $existingHash -ne $receipt.sha256)) {
            throw (Text 'Another xinput1_3.dll already exists. The installer will not overwrite it.' 'Уже найден другой xinput1_3.dll. Установщик не будет его перезаписывать.')
        }
    }
    if ($Remove) {
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target }
        if (Test-Path -LiteralPath $receiptPath) { Remove-Item -LiteralPath $receiptPath }
        Write-Host (Text 'Simple Mods was removed. Your mods and settings were preserved.' 'Загрузчик удалён. Папка модов и ваши настройки сохранены.')
    } else {
        $expectedGameHash = 'AA057BF03B911838528B2F264009AE2484F2FAE50072A15F312F72645CCB1EA6'
        if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $expectedGameHash) {
            throw (Text 'This installer supports only the GOG build of Hello Neighbor listed in the README.' 'Эта версия установщика рассчитана на GOG-сборку Hello Neighbor, указанную в README.')
        }
        New-Item -ItemType Directory -Force -Path (Join-Path $stateDir 'mods') | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Force
        $settingsPath = Join-Path $stateDir 'settings.ini'
        if (-not (Test-Path -LiteralPath $settingsPath)) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'payload\settings.ini') -Destination $settingsPath
        }
        @{ product='SimpleMods'; version='0.4.0'; sha256=$sourceHash } |
            ConvertTo-Json | Set-Content -LiteralPath $receiptPath -Encoding UTF8
        Write-Host (Text 'Simple Mods is installed. Start the game as usual.' 'Simple Mods установлен. Запустите игру обычным способом.')
    }
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
