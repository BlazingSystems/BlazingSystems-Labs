@echo off
cd /d "%~dp0"
call .venv\Scripts\activate
python tools\download_sd15.py
pause
