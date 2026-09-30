@echo off
cd /d "%~dp0"
where py >nul 2>nul || (echo Install Python 3.11+ first.& pause& exit /b 1)
py -3 run_gui.py
