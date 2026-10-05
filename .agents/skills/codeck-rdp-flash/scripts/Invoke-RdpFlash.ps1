param(
    [Parameter(Mandatory = $true)][ValidatePattern('^COM[1-9][0-9]*$')][string]$Port,
    [ValidateSet('full', 'app')][string]$Mode = 'full',
    [ValidateSet(460800, 115200)][int]$Baud = 460800,
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$buildRoot = Join-Path $projectRoot 'build'
$pythonExe = 'D:\Espressif\tools\python\v6.1\venv\Scripts\python.exe'
$metadata = Get-Content -LiteralPath (Join-Path $buildRoot 'flasher_args.json') -Raw | ConvertFrom-Json
if ($metadata.extra_esptool_args.chip -ne 'esp32s3') { throw 'Unexpected chip in build metadata.' }
if ($metadata.flash_settings.flash_size -ne '16MB') { throw 'Unexpected flash size.' }
$expectedFiles = @{'0x0'='bootloader/bootloader.bin'; '0x8000'='partition_table/partition-table.bin'; '0x10000'='codeck.bin'}
$actualFiles = @($metadata.flash_files.PSObject.Properties)
if ($actualFiles.Count -ne 3) { throw 'Unexpected flash layout; review NVS preservation before changing this helper.' }
foreach ($entry in $actualFiles) {
    if ($expectedFiles[$entry.Name] -ne $entry.Value) { throw 'Unexpected flash offsets or files; review layout before writing.' }
    $artifact = Join-Path $buildRoot $entry.Value
    if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) { throw "Missing artifact: $artifact" }
    $artifactSize = (Get-Item -LiteralPath $artifact).Length
    if ($artifactSize -eq 0) { throw "Empty artifact: $artifact" }
    if ($entry.Name -eq '0x0' -and $artifactSize -gt 0x8000) { throw 'Bootloader overlaps partition table.' }
    if ($entry.Name -eq '0x8000' -and $artifactSize -gt 0x1000) { throw 'Partition table overlaps NVS.' }
}
$appSize = (Get-Item -LiteralPath (Join-Path $buildRoot 'codeck.bin')).Length
if ($appSize -gt 2MB) { throw 'Application exceeds current 2 MiB factory partition.' }
if (-not (Test-Path -LiteralPath $pythonExe)) { throw "Missing ESP-IDF Python: $pythonExe" }
$flashArguments = @('-m', 'esptool', '--chip', 'esp32s3', '--port', $Port, '--baud', "$Baud", '--before', 'no-reset', '--after', 'hard-reset', 'write-flash')
$flashArguments += @($metadata.write_flash_args)
foreach ($offset in @('0x0', '0x8000', '0x10000')) {
    if ($Mode -eq 'app' -and $offset -ne '0x10000') { continue }
    $flashArguments += @($offset, $expectedFiles[$offset])
}
Write-Output "Project: $projectRoot; mode: $Mode; port: $Port; app bytes: $appSize"
if ($DryRun) {
    Write-Output ($pythonExe + ' ' + ($flashArguments -join ' '))
    Write-Output 'Dry run: no serial port opened, no flash written. Confirm build freshness and physical download mode before execution.'
    exit 0
}
$env:PYTHONUTF8 = '1'
Push-Location $buildRoot
try {
    & $pythonExe @flashArguments
    $flashExit = $LASTEXITCODE
} finally { Pop-Location }
exit $flashExit
