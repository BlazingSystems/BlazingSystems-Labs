@echo off
cd /d "%~dp0"
if exist .venv\Scripts\python.exe (.venv\Scripts\python.exe backend\server.py) else (python backend\server.py)
pause
