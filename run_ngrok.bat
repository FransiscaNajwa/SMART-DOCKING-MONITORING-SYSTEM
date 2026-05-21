@echo off
setlocal
rem ====================================================================
rem  ngrok tunnel launcher for the Smart Berthing System server
rem  This script starts an HTTP tunnel on port 80 (Laragon default).
rem  It first tries ngrok.exe in the same folder as this batch file,
rem  then falls back to ngrok available in PATH.
rem ====================================================================

set "SCRIPT_DIR=%~dp0"
set "NGROK_EXE=%SCRIPT_DIR%ngrok.exe"

if exist "%NGROK_EXE%" (
    "%NGROK_EXE%" http 80
    exit /b %errorlevel%
)

where ngrok >nul 2>nul
if %errorlevel%==0 (
    ngrok http 80
    exit /b %errorlevel%
)

echo [ERROR] ngrok.exe tidak ditemukan.
echo Letakkan ngrok.exe di folder ini:
echo   %SCRIPT_DIR%
echo atau tambahkan ngrok ke PATH Windows.
echo Download: https://ngrok.com/download
pause
exit /b 1
