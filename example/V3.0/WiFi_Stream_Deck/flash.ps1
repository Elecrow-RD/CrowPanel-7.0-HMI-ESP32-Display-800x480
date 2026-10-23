param([string]$Port)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

if (-not $Port) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if (-not $ports) {
        throw "No COM port found. Plug in the display's USB-C cable, then run this again."
    }
    Write-Host "COM ports on this PC: $($ports -join ', ')"
    Write-Host ""
    Write-Host "Use the UART/CH340 port. The native USB port cannot be flashed while"
    Write-Host "this firmware runs, because GPIO19/20 drive the touch I2C bus."
    Write-Host ""
    $Port = Read-Host "Which one is the display?"
}

Write-Host "Building and flashing to $Port ..."
python -m platformio run -e crowpanel_7 -t upload --upload-port $Port
