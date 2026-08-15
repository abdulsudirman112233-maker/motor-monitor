@echo off
title Smart Vehicle IoT Dashboard Server
echo ============================================================
echo   Menjalankan Server Web Dashboard IoT...
echo ============================================================
start "" "http://localhost:3000"
powershell -ExecutionPolicy Bypass -File "%~dp0dashboard\serve.ps1"
pause
