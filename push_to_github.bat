@echo off
title Push Proyek ke GitHub (motor-monitor)
cls
echo ============================================================
echo   MENGUNGGAH PROYEK KE GITHUB REPOSITORY
echo   Target: https://github.com/abdulsudirman112233-maker/motor-monitor.git
echo ============================================================
echo.
cd /d "%~dp0"

echo [1/3] Memeriksa status file...
git add -A
git commit -m "IoT smart motorcycle security, dual-mode GSM/WiFi failover, real GPS tracking & dashboard" >nul 2>&1
git branch -M main
git remote set-url origin https://github.com/abdulsudirman112233-maker/motor-monitor.git

echo [2/3] Mengunggah file ke GitHub (git push -u origin main)...
echo.
echo * Catatan: Jika muncul jendela popup login GitHub, silakan klik 'Sign in with your browser'.
echo.
git push -u origin main

echo.
if %ERRORLEVEL% EQU 0 (
    echo ============================================================
    echo   [SUKSES] Seluruh file berhasil terunggah ke GitHub!
    echo   Cek di: https://github.com/abdulsudirman112233-maker/motor-monitor
    echo ============================================================
) else (
    echo ============================================================
    echo   [INFO] Jika gagal autentikasi:
    echo   1. Pastikan Anda sudah login ke akun GitHub Anda.
    echo   2. Atau gunakan Personal Access Token (PAT) sebagai password.
    echo ============================================================
)
echo.
pause
