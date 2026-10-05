param([switch]$DryRun)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$idfRoot = 'D:\esp\v6.1\esp-idf'
$pythonExe = 'D:\Espressif\tools\python\v6.1\venv\Scripts\python.exe'
$toolBins = @(
    'D:\Espressif\tools\python\v6.1\venv\Scripts',
    'D:\Espressif\tools\cmake\4.0.3\bin',
    'D:\Espressif\tools\ninja\1.12.1',
    'D:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin'
)
foreach ($requiredPath in @($pythonExe, "$idfRoot\tools\idf.py") + $toolBins) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Missing ESP-IDF tool: $requiredPath. Configure installed paths before continuing." }
}
if ($DryRun) {
    Write-Output "Project: $projectRoot"
    Write-Output "Build: $pythonExe $idfRoot\tools\idf.py build"
    exit 0
}
$env:PYTHONUTF8 = '1'
$env:ESP_IDF_VERSION = '6.1.0'
$env:ESP_ROM_ELF_DIR = 'D:/Espressif/tools/esp-rom-elfs/20241011'
$env:IDF_PATH = $idfRoot
$env:IDF_TOOLS_PATH = 'D:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH = 'D:\Espressif\tools\python\v6.1\venv'
$env:PATH = ($toolBins -join ';') + ';' + $env:PATH
# Process-local Git settings: do not modify global trust configuration.
$env:GIT_CONFIG_COUNT = '3'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = $idfRoot.Replace('\', '/')
$env:GIT_CONFIG_KEY_1 = 'safe.directory'
$env:GIT_CONFIG_VALUE_1 = $projectRoot.Replace('\', '/')
$env:GIT_CONFIG_KEY_2 = 'safe.directory'
$env:GIT_CONFIG_VALUE_2 = "$($idfRoot.Replace('\', '/'))/components/openthread/openthread"
Push-Location $projectRoot
try {
    & $pythonExe "$idfRoot\tools\idf.py" build
    $buildExit = $LASTEXITCODE
} finally { Pop-Location }
exit $buildExit
