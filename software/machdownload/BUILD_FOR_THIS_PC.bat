@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
echo === MachDownload native-machine build ===
where py >nul 2>nul || (echo Python 3.11+ is required. Install from python.org and retry.& pause& exit /b 1)
py -3 -m venv .buildenv || exit /b 1
call .buildenv\Scripts\activate.bat
python -m pip install --upgrade pip nuitka ordered-set zstandard || exit /b 1
echo Detecting CPU...
powershell -NoProfile -Command "$c=Get-CimInstance Win32_Processor|Select-Object -First 1; 'CPU: '+$c.Name; 'Cores: '+$c.NumberOfCores; 'Threads: '+$c.NumberOfLogicalProcessors" > native-build-info.txt
type native-build-info.txt
echo.
echo Nuitka compiles Python modules to native C and links an EXE for this Windows environment.
echo The resulting build is intentionally not promised portable to other PCs.
python -m nuitka --onefile --windows-console-mode=disable --enable-plugin=tk-inter --assume-yes-for-downloads --lto=yes --output-filename=MachDownload_Native.exe run_gui.py || exit /b 1
if not exist dist mkdir dist
move /Y MachDownload_Native.exe dist\MachDownload_Native.exe >nul
copy /Y native-build-info.txt dist\native-build-info.txt >nul
echo.
echo Built: dist\MachDownload_Native.exe
pause
