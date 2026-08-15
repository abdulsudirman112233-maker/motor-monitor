# PowerShell Script: Pengirim Data Uji Coba ke Firebase Realtime Database
# Project: motor-monitor-9f391

param(
    [double]$lat = -6.2088,
    [double]$lng = 106.8456,
    [double]$speed = 35.5,
    [double]$battery = 12.6,
    [bool]$vibration = $false,
    [bool]$engineRunning = $true,
    [int]$satellites = 9,
    [string]$vehicleId = "vehicle_01"
)

$firebaseUrl = "https://motor-monitor-9f391-default-rtdb.asia-southeast1.firebasedatabase.app/vehicles/$vehicleId/telemetry.json"
$statusUrl   = "https://motor-monitor-9f391-default-rtdb.asia-southeast1.firebasedatabase.app/vehicles/$vehicleId/status.json"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " 🚀 MENGIRIM DATA TELEMETRI KE FIREBASE REALTIME DATABASE..." -ForegroundColor Yellow
Write-Host " Target URL: $firebaseUrl" -ForegroundColor Gray
Write-Host "============================================================" -ForegroundColor Cyan

$telemetryPayload = @{
    latitude           = $lat
    longitude          = $lng
    altitude           = 18.5
    speed              = $speed
    heading            = 45.0
    satellites         = $satellites
    hdop               = 1.2
    gps_fixed          = $true
    gsm_csq            = 26
    gsm_signal_percent = 85
    gsm_network        = "TELKOMSEL"
    battery_voltage    = $battery
    power_source       = "ACCU_12V"
    vibration_detected = $vibration
    engine_running     = $engineRunning
    connection_mode    = "WIFI_ONLINE"
    timestamp          = [int][double]::Parse((Get-Date -UFormat %s))
} | ConvertTo-Json

try {
    $response = Invoke-RestMethod -Uri $firebaseUrl -Method Patch -Body $telemetryPayload -ContentType "application/json"
    Write-Host " [OK] Telemetri Berhasil Terkirim ke Firebase!" -ForegroundColor Green
    Write-Host " Data Terkirim:" -ForegroundColor Cyan
    Write-Host "   - Koordinat : $lat, $lng"
    Write-Host "   - Kecepatan : $speed km/h"
    Write-Host "   - Aki Motor : $battery V"
    Write-Host "   - Getaran   : $vibration"
    Write-Host "   - Mesin     : $(if($engineRunning){'Hidup'}else{'Mati'})"
    Write-Host "------------------------------------------------------------" -ForegroundColor Gray
    Write-Host " 🌐 Cek Live di Web Dashboard: http://localhost:3000" -ForegroundColor Yellow
    Write-Host "============================================================" -ForegroundColor Cyan
} catch {
    Write-Host " [GAGAL] Terjadi kesalahan: $_" -ForegroundColor Red
}
