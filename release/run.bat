@echo off
if "%~1"=="" (
    echo Drag a CHIP-8 ROM onto this file.
    pause
    exit /b
)

chip8.exe "%~1"
