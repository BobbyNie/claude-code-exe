@echo off
REM Claude Code Portable Launcher
REM This script sets up the environment variables for portable mode
REM and then launches the Claude Code executable.

REM Set portable directories relative to this script
set CLAUDE_CONFIG_DIR=%~dp0data\.claude
set CLAUDE_DATA_DIR=%~dp0data\claude

REM Create data directories if they don't exist
if not exist "%~dp0data" mkdir "%~dp0data"
if not exist "%CLAUDE_CONFIG_DIR%" mkdir "%CLAUDE_CONFIG_DIR%"
if not exist "%CLAUDE_DATA_DIR%" mkdir "%CLAUDE_DATA_DIR%"

REM Launch Claude Code with any passed arguments
start "" "%~dp0claude.exe" %*
