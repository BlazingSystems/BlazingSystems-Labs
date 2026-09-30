@echo off
setlocal
cd /d "%~dp0"
start "MachDownload test server" /min py -3 tests\range_server.py 8765
timeout /t 2 /nobreak >nul
py -3 tests\selftest.py
set ERR=%ERRORLEVEL%
taskkill /FI "WINDOWTITLE eq MachDownload test server*" /T /F >nul 2>nul
if %ERR% NEQ 0 (echo TEST FAILED& pause& exit /b %ERR%)
echo TEST PASSED
pause
