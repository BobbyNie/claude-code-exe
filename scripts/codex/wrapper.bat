@echo off
REM Codex CLI Portable Launcher
REM This script sets CODEX_HOME to keep Codex CLI data inside the release folder.

set "CODEX_HOME=%~dp0data\.codex"

if not exist "%~dp0data" mkdir "%~dp0data"
if not exist "%CODEX_HOME%" mkdir "%CODEX_HOME%"

"%~dp0codex.exe" %*
