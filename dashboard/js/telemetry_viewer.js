// =============================================================================
// TELEMETRY & HUD VIEWER - GAUGES, STATS & LOG TABLE CONTROLLER
// =============================================================================

class TelemetryViewer {
    constructor() {
        this.maxSpeedMeter = 140; // km/h skala maksimum
        this.svgArcLength = 283;  // Sesuai stroke-dasharray SVG
    }

    updateSpeedometer(speedKmh) {
        const speedVal = Math.max(0, Math.min(speedKmh || 0, this.maxSpeedMeter));
        
        // Update Teks Angka
        const speedEl = document.getElementById('speedNumber');
        if (speedEl) {
            speedEl.textContent = speedVal.toFixed(0);
        }

        // Update Arc SVG Progress
        const arcProgressEl = document.getElementById('speedMeterProgress');
        if (arcProgressEl) {
            const fraction = speedVal / this.maxSpeedMeter;
            const offset = this.svgArcLength * (1 - fraction);
            arcProgressEl.style.strokeDashoffset = offset;
        }
    }

    updateTelemetryCards(telemetry, status) {
        if (!telemetry) return;

        // 1. Update Speedometer
        this.updateSpeedometer(telemetry.speed);

        // 2. Satelit GPS
        const satsEl = document.getElementById('statSatellites');
        if (satsEl) {
            satsEl.textContent = `${telemetry.satellites || 0} Sats (${telemetry.hdop ? telemetry.hdop.toFixed(1) : '1.0'} HDOP)`;
        }

        // 3. Sinyal GSM & CSQ
        const gsmEl = document.getElementById('statGsmSignal');
        if (gsmEl) {
            const percent = telemetry.gsm_signal_percent || 0;
            const csq = telemetry.gsm_csq || 0;
            gsmEl.textContent = `${percent}% (CSQ ${csq})`;
        }

        // 4. Tegangan Baterai / Aki
        const battEl = document.getElementById('statBattery');
        if (battEl) {
            const v = telemetry.battery_voltage || 12.6;
            battEl.textContent = `${v.toFixed(1)} V (${telemetry.power_source || 'ACCU_12V'})`;
            battEl.style.color = (v < 11.2) ? 'var(--accent-red)' : 'var(--text-primary)';
        }

        // 5. Sensor Getar SW-420
        const vibEl = document.getElementById('statVibration');
        if (vibEl) {
            const isVib = telemetry.vibration_detected || (status && status.theft_alert);
            vibEl.textContent = isVib ? 'GETARAN TERDETEKSI' : 'STABIL (AMAN)';
            vibEl.style.color = isVib ? 'var(--accent-red)' : 'var(--accent-green)';
        }

        // 6. Status Kontak & Mesin
        const engineStatusEl = document.getElementById('statEngineRunning');
        if (engineStatusEl) {
            if (status && status.engine_locked) {
                engineStatusEl.textContent = 'CUT-OFF (TERPUTUS)';
                engineStatusEl.style.color = 'var(--accent-red)';
            } else if (telemetry.speed > 2.0) {
                engineStatusEl.textContent = 'BERJALAN (ON)';
                engineStatusEl.style.color = 'var(--accent-green)';
            } else {
                engineStatusEl.textContent = 'STANDBY / IDLE';
                engineStatusEl.style.color = 'var(--text-secondary)';
            }
        }

        // 7. Status Badges Header Topbar
        this.updateHeaderBadges(telemetry, status);
    }

    updateHeaderBadges(telemetry, status) {
        // Status Koneksi Jaringan
        const netBadge = document.getElementById('badgeConnection');
        if (netBadge) {
            const isOnline = telemetry.connection_mode !== 'OFFLINE';
            netBadge.innerHTML = `
                <span class="pulse-dot ${isOnline ? '' : 'danger'}"></span>
                <span>${telemetry.connection_mode || 'WIFI_ONLINE'}</span>
            `;
        }

        // Status Keamanan (ARMED / DISARMED)
        const armBadge = document.getElementById('badgeSecurityMode');
        const btnArm = document.getElementById('btnToggleArm');
        if (armBadge) {
            const isArmed = status ? status.armed : true;
            armBadge.innerHTML = `
                <i class="fa-solid ${isArmed ? 'fa-shield-halved' : 'fa-shield'}"></i>
                <span>${isArmed ? 'ARMED' : 'DISARMED'}</span>
            `;
            armBadge.style.color = isArmed ? 'var(--accent-green)' : 'var(--text-secondary)';
            
            if (btnArm) {
                btnArm.dataset.armed = isArmed ? 'true' : 'false';
                btnArm.innerHTML = isArmed ? 
                    '<i class="fa-solid fa-shield-halved"></i> ARMED' : 
                    '<i class="fa-solid fa-shield"></i> DISARMED';
                btnArm.classList.toggle('active', isArmed);
            }
        }

        // Status Engine Ignition & Tombol Kill Switch
        const ignBadge = document.getElementById('badgeEngineStatus');
        const btnEngine = document.getElementById('btnToggleEngine');
        const boxEngine = document.getElementById('engineKillBox');
        if (ignBadge) {
            const isLocked = status ? status.engine_locked : false;
            ignBadge.innerHTML = `
                <i class="fa-solid ${isLocked ? 'fa-ban' : 'fa-bolt'}"></i>
                <span>${isLocked ? 'ENGINE CUT-OFF' : 'ENGINE NORMAL'}</span>
            `;
            ignBadge.style.color = isLocked ? 'var(--accent-red)' : 'var(--primary)';

            if (btnEngine) {
                btnEngine.dataset.locked = isLocked ? 'true' : 'false';
                btnEngine.innerHTML = isLocked ? 
                    '<i class="fa-solid fa-key"></i> PULIHKAN MESIN' : 
                    '<i class="fa-solid fa-ban"></i> MATIKAN MESIN';
                btnEngine.className = isLocked ? 'btn-engine-toggle unlocked-state' : 'btn-engine-toggle';
            }

            if (boxEngine) {
                boxEngine.classList.toggle('locked', isLocked);
            }
        }
    }

    appendLogEntry(logData) {
        const tbody = document.getElementById('logsTableBody');
        if (!tbody) return;

        const tr = document.createElement('tr');
        
        let badgeClass = 'badge-info';
        if (logData.event_type.includes('THEFT') || logData.event_type.includes('ALARM') || logData.event_type.includes('KILL')) {
            badgeClass = 'badge-danger';
        } else if (logData.event_type.includes('ARM') || logData.event_type.includes('RESTORE')) {
            badgeClass = 'badge-success';
        } else if (logData.event_type.includes('WARN') || logData.event_type.includes('GEOFENCE')) {
            badgeClass = 'badge-warning';
        }

        const timeStr = logData.datetime || new Date().toLocaleTimeString('id-ID');
        const latLngStr = (logData.latitude && logData.longitude) ? 
            `${logData.latitude.toFixed(5)}, ${logData.longitude.toFixed(5)}` : '-';
        const spdStr = (logData.speed !== undefined) ? `${logData.speed.toFixed(1)} km/h` : '-';

        tr.innerHTML = `
            <td>${timeStr}</td>
            <td><span class="log-badge ${badgeClass}">${logData.event_type}</span></td>
            <td>${logData.message || '-'}</td>
            <td>${latLngStr}</td>
            <td>${spdStr}</td>
        `;

        // Sisipkan di baris teratas
        tbody.insertBefore(tr, tbody.firstChild);

        // Batasi maksimal 50 baris di tabel
        if (tbody.children.length > 50) {
            tbody.removeChild(tbody.lastChild);
        }
    }
}
