@echo off
setlocal
cd /d "%~dp0"
py -3 -m venv .buildenv || exit /b 1
call .buildenv\Scripts\activate.bat
python -m pip install --upgrade pip pyinstaller || exit /b 1
pyinstaller --noconfirm --clean --onefile --console --name MachDownload_CLI run_cli.py || exit /b 1
pause
