@echo off
title Deploy Web Dashboard ke Vercel (motor-monitor)
cls
echo ============================================================
echo   DEPLOY WEB DASHBOARD KE VERCEL CLOUD
echo ============================================================
echo.
cd /d "%~dp0"

echo Menjalankan Vercel CLI Deployment...
echo * Jika belum login ke Vercel, browser akan meminta konfirmasi login akun Vercel Anda.
echo.
npx -y vercel --prod

echo.
if %ERRORLEVEL% EQU 0 (
    echo ============================================================
    echo   [SUKSES] Web Dashboard Berhasil Di-deploy ke Vercel!
    echo   Web dashboard sekarang aktif secara publik dengan HTTPS.
    echo ============================================================
) else (
    echo [INFO] Periksa koneksi internet atau login Vercel Anda.
)
echo.
pause
