@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-audio-key.ps1"
exit /b %errorlevel%
