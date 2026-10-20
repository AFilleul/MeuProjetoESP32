@echo off
rem Abre os fluxos dos firmwares no navegador com um servidor local (precisa so do PowerShell do Windows).
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0servidor.ps1"
pause
