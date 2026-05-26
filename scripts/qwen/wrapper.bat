@echo off
REM Qwen Code Portable Launcher
REM This script sets up portable environment variables and launches qwen.exe.

set "QWEN_HOME=%~dp0data\.qwen"
set "QWEN_RUNTIME_DIR=%~dp0data\qwen-runtime"

if not exist "%~dp0data" mkdir "%~dp0data"
if not exist "%QWEN_HOME%" mkdir "%QWEN_HOME%"
if not exist "%QWEN_RUNTIME_DIR%" mkdir "%QWEN_RUNTIME_DIR%"

"%~dp0qwen.exe" %*
