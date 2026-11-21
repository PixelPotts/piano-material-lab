@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\bootstrap-compiler.ps1
if errorlevel 1 exit /b 1
compiler\zig-windows-x86_64-0.13.0\zig.exe cc -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wl,--subsystem,windows piano.c -o piano.exe -luser32 -lgdi32 -lwinmm
if errorlevel 1 exit /b 1
echo Built piano.exe with Clang via Zig
