@echo off
rem Double-click launcher: works even when PowerShell script execution is restricted.
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1"
if errorlevel 1 (
  echo.
  echo Server exited with an error. Copy the message above.
  pause
)
endlocal
