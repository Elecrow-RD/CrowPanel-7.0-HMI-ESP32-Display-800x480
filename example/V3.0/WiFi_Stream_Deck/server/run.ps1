$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

if (-not (Test-Path ".venv")) {
    py -m venv .venv
}

& ".\.venv\Scripts\python.exe" -m pip install -q -r requirements.txt

Write-Host ""
Write-Host "Listening on 0.0.0.0:8787. Local addresses:"
Get-NetIPAddress -AddressFamily IPv4 |
    Where-Object { $_.IPAddress -notmatch '^(127\.|169\.254\.)' } |
    ForEach-Object { Write-Host ("  {0,-32} {1}" -f $_.InterfaceAlias, $_.IPAddress) }
Write-Host "SERVER_HOST in include/secrets.h must be one of the addresses above."
Write-Host ""

& ".\.venv\Scripts\python.exe" -m uvicorn server:app --host 0.0.0.0 --port 8787
