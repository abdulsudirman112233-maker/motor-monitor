@echo off
title Kirim Data Uji Coba ke Firebase (motor-monitor-9f391)
cls
echo ============================================================
echo   TOOL PENGIRIM DATA CEPAT KE FIREBASE REALTIME DATABASE
echo ============================================================
echo.
set /p lat="Masukkan Latitude (Default: -6.2088) : " || set lat=-6.2088
if "%lat%"=="" set lat=-6.2088

set /p lng="Masukkan Longitude (Default: 106.8456) : " || set lng=106.8456
if "%lng%"=="" set lng=106.8456

set /p spd="Masukkan Kecepatan km/h (Default: 45.5) : " || set spd=45.5
if "%spd%"=="" set spd=45.5

set /p batt="Masukkan Tegangan Aki (Default: 12.6) : " || set batt=12.6
if "%batt%"=="" set batt=12.6

echo.
echo Mengirim data ke Firebase...
powershell -ExecutionPolicy Bypass -Command "& '%~dp0kirim_data_test.ps1' -lat %lat% -lng %lng% -speed %spd% -battery %batt%"
echo.
pause
