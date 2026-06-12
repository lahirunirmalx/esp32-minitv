# Build + flash + monitor for the ESP32 Mini TV — Windows (PowerShell).
#
#   .\scripts\flash.ps1            # build, flash, monitor
#   .\scripts\flash.ps1 build      # build only
#   .\scripts\flash.ps1 monitor    # monitor only
#
# Prerequisite: ESP-IDF v5.5 via the official Windows installer
# (https://dl.espressif.com/dl/esp-idf/ -> pick v5.5). The installer also
# sets up the CH340 USB driver. Exit monitor with Ctrl+].
param([string]$Mode = "all")

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

# ---- locate ESP-IDF ---------------------------------------------------------
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    $candidates = @()
    if ($env:IDF_PATH) { $candidates += $env:IDF_PATH }
    $candidates += "$env:USERPROFILE\esp\v5.5\esp-idf"
    $candidates += (Get-ChildItem "C:\Espressif\frameworks\esp-idf-v5.5*" -Directory -ErrorAction SilentlyContinue | ForEach-Object FullName)

    $idf = $candidates | Where-Object { Test-Path "$_\export.ps1" } | Select-Object -First 1
    if (-not $idf) {
        Write-Error @"
ESP-IDF v5.5 not found.
Install it with the official Windows installer:
  https://dl.espressif.com/dl/esp-idf/   (choose v5.5)
then either re-run this script, or use the 'ESP-IDF 5.5 PowerShell' start-menu
shortcut and run:  idf.py -p COMx build flash monitor
"@
    }
    Write-Host "using ESP-IDF at $idf"
    . "$idf\export.ps1" | Out-Null
}

Set-Location $ProjectDir

if ($Mode -eq "build") { idf.py build; exit $LASTEXITCODE }

# ---- find the CH340 COM port ------------------------------------------------
$port = (Get-CimInstance Win32_PnPEntity |
         Where-Object { $_.Name -match "CH340|USB-SERIAL" -and $_.Name -match "\(COM\d+\)" } |
         ForEach-Object { [regex]::Match($_.Name, "COM\d+").Value } |
         Select-Object -First 1)
if (-not $port) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 1) { $port = $ports[0] }
}
if (-not $port) {
    Write-Error @"
No serial port found — is the Mini TV plugged in with a data-capable USB cable?
If Device Manager shows an unknown device, install the CH340 driver
(the ESP-IDF Windows installer includes it, or get CH341SER from wch.cn).
"@
}
Write-Host "using port: $port"

if ($Mode -eq "monitor") { idf.py -p $port monitor; exit $LASTEXITCODE }

idf.py -p $port build flash monitor
exit $LASTEXITCODE
