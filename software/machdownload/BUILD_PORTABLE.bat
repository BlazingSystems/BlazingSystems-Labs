@echo off
setlocal
cd /d "%~dp0"
echo === MachDownload portable build ===
where py >nul 2>nul || (echo Python 3.11+ is required. Install from python.org and retry.& pause& exit /b 1)
py -3 -m venv .buildenv || exit /b 1
call .buildenv\Scripts\activate.bat
python -m pip install --upgrade pip pyinstaller || exit /b 1
if exist dist rmdir /s /q dist
pyinstaller --noconfirm --clean --onefile --windowed --name MachDownload_Portable run_gui.py || exit /b 1
echo.
echo Built: dist\MachDownload_Portable.exe
pause
