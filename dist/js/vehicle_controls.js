// =============================================================================
// VEHICLE CONTROLS & COMMAND DISPATCHER (DUAL-DISPATCH: SDK + REST API)
// =============================================================================

class VehicleControls {
    constructor(firebaseApp, telemetryViewer, mapController) {
        this.firebaseApp = firebaseApp;
        this.telemetryViewer = telemetryViewer;
        this.mapController = mapController;
        this.isSimMode = false;
        this.audioContext = null;
        this.sirenOscillator = null;
        this.isSirenPlaying = false;

        // Properti Sistem Pembatasan Jarak / Geofence
        this.geofenceRadius = 20;
        this.geofenceEnabled = true;
        this.autoCutoffGeofence = true;
        this.anchorLat = null;
        this.anchorLng = null;
        this._geofenceDebounceTimer = null;
    }

    init() {
        this._bindUIEvents();
        this._bindGeofenceControls();
        this._initAudio();
        console.log('[CONTROLS] Vehicle Controls & Geofence System Initialized.');
    }

    _initAudio() {
        try {
            const AudioCtx = window.AudioContext || window.webkitAudioContext;
            this.audioContext = new AudioCtx();
        } catch (e) {
            console.warn('[AUDIO] Web Audio API not supported on this browser.');
        }
    }

    playChirp(count = 1) {
        if (!this.audioContext) return;
        if (this.audioContext.state === 'suspended') {
            this.audioContext.resume();
        }

        for (let i = 0; i < count; i++) {
            setTimeout(() => {
                const osc = this.audioContext.createOscillator();
                const gain = this.audioContext.createGain();

                osc.type = 'sine';
                osc.frequency.setValueAtTime(APP_CONFIG.AUDIO.BEEP_FREQ, this.audioContext.currentTime);

                gain.gain.setValueAtTime(0.3, this.audioContext.currentTime);
                gain.gain.exponentialRampToValueAtTime(0.01, this.audioContext.currentTime + 0.08);

                osc.connect(gain);
                gain.connect(this.audioContext.destination);

                osc.start();
                osc.stop(this.audioContext.currentTime + 0.08);
            }, i * 140);
        }
    }

    startBrowserSiren() {
        if (this.isSirenPlaying) return;
        if (!this.audioContext) return;
        if (this.audioContext.state === 'suspended') {
            this.audioContext.resume();
        }

        this.isSirenPlaying = true;
        const now = this.audioContext.currentTime;

        this.sirenOscillator = this.audioContext.createOscillator();
        const sirenGain = this.audioContext.createGain();

        this.sirenOscillator.type = 'sawtooth';
        sirenGain.gain.setValueAtTime(0.25, now);

        // Modulasi nada sirene (polisi/darurat)
        this.sirenOscillator.frequency.setValueAtTime(APP_CONFIG.AUDIO.SIREN_LOW_FREQ, now);
        this.sirenOscillator.frequency.linearRampToValueAtTime(APP_CONFIG.AUDIO.SIREN_HIGH_FREQ, now + 0.5);

        this.sirenInterval = setInterval(() => {
            if (!this.isSirenPlaying) return;
            const t = this.audioContext.currentTime;
            this.sirenOscillator.frequency.setValueAtTime(APP_CONFIG.AUDIO.SIREN_LOW_FREQ, t);
            this.sirenOscillator.frequency.linearRampToValueAtTime(APP_CONFIG.AUDIO.SIREN_HIGH_FREQ, t + 0.45);
        }, 500);

        this.sirenOscillator.connect(sirenGain);
        sirenGain.connect(this.audioContext.destination);
        this.sirenOscillator.start();
    }

    stopBrowserSiren() {
        if (!this.isSirenPlaying) return;
        this.isSirenPlaying = false;
        if (this.sirenInterval) clearInterval(this.sirenInterval);
        if (this.sirenOscillator) {
            try {
                this.sirenOscillator.stop();
                this.sirenOscillator.disconnect();
            } catch (e) {}
            this.sirenOscillator = null;
        }
    }

    _bindUIEvents() {
        // 1. Tombol Engine Kill Switch
        const btnEngine = document.getElementById('btnToggleEngine');
        if (btnEngine) {
            btnEngine.addEventListener('click', () => {
                const currentLockState = btnEngine.dataset.locked === 'true';
                const nextLockState = !currentLockState;
                this.setEngineLock(nextLockState);
            });
        }

        // 2. Tombol Arm / Disarm System
        const btnArm = document.getElementById('btnToggleArm');
        if (btnArm) {
            btnArm.addEventListener('click', () => {
                const currentArmed = btnArm.dataset.armed === 'true';
                this.setArmSystem(!currentArmed);
            });
        }

        // 3. Tombol Panic Siren
        const btnPanic = document.getElementById('btnPanicSiren');
        if (btnPanic) {
            btnPanic.addEventListener('click', () => {
                this.triggerPanicAlarm();
            });
        }

        // 4. Tombol Find Vehicle (Chirp)
        const btnFind = document.getElementById('btnFindVehicle');
        if (btnFind) {
            btnFind.addEventListener('click', () => {
                this.triggerFindVehicle();
            });
        }

        // 5. Tombol Request Emergency SMS
        const btnSms = document.getElementById('btnRequestSms');
        if (btnSms) {
            btnSms.addEventListener('click', () => {
                this.requestEmergencySMS();
            });
        }

        // 6. Tombol Modal Dismiss & Reset
        const btnResetAlarm = document.getElementById('btnResetAlarmModal');
        if (btnResetAlarm) {
            btnResetAlarm.addEventListener('click', () => {
                this.resetTheftAlarm();
            });
        }
    }

    setEngineLock(lockState) {
        this.playChirp(lockState ? 1 : 2);
        
        const btnEngine = document.getElementById('btnToggleEngine');
        const boxEngine = document.getElementById('engineKillBox');
        if (btnEngine) {
            btnEngine.dataset.locked = lockState ? 'true' : 'false';
            btnEngine.innerHTML = lockState ? 
                '<i class="fa-solid fa-key"></i> PULIHKAN MESIN' : 
                '<i class="fa-solid fa-ban"></i> MATIKAN MESIN';
            btnEngine.className = lockState ? 'btn-engine-toggle unlocked-state' : 'btn-engine-toggle';
        }
        if (boxEngine) {
            boxEngine.classList.toggle('locked', lockState);
        }

        this._dispatchFirebaseCommand('lock_engine', lockState);
        this._showToast(lockState ? 'Perintah: Mesin Telah Dimatikan (Relay Cut-Off)' : 'Perintah: Pengapian Mesin Dipulihkan', lockState ? 'error' : 'success');
    }

    setArmSystem(armedState) {
        this.playChirp(armedState ? 1 : 2);
        
        const btnArm = document.getElementById('btnToggleArm');
        if (btnArm) {
            btnArm.dataset.armed = armedState ? 'true' : 'false';
            btnArm.innerHTML = armedState ? 
                '<i class="fa-solid fa-shield-halved"></i> ARMED (TERKUNCI)' : 
                '<i class="fa-solid fa-shield"></i> DISARMED (BUKA)';
            if (armedState) btnArm.classList.add('active');
            else btnArm.classList.remove('active');
        }

        this._dispatchFirebaseCommand('armed', armedState);
        this._showToast(armedState ? 'Sistem Keamanan di-ARM (Siaga)' : 'Sistem Keamanan di-DISARM (Terbuka)', 'info');
    }

    triggerPanicAlarm() {
        this.startBrowserSiren();
        this._dispatchFirebaseCommand('trigger_panic', true);
        this._showToast('Sirene Darurat Dinyalakan!', 'error');

        // Tampilkan Modal Peringatan Darurat
        const modal = document.getElementById('emergencyAlertModal');
        if (modal) modal.classList.add('active');
    }

    triggerFindVehicle() {
        this.playChirp(3);
        this._dispatchFirebaseCommand('find_vehicle', true);
        this._showToast('Sinyal Locator Dikirim (Buzzer Berbunyi)', 'info');
    }

    requestEmergencySMS() {
        const btnSms = document.getElementById('btnRequestSms');
        const origContent = btnSms ? btnSms.innerHTML : '';
        if (btnSms) {
            btnSms.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> MENGIRIM SMS...';
            btnSms.disabled = true;
        }

        this._dispatchFirebaseCommand('emergency_sms_request', true);
        this._showToast('Permintaan Kirim SMS SOS Berhasil Dikirim ke Cloud Firebase!', 'warning');

        setTimeout(() => {
            if (btnSms) {
                btnSms.innerHTML = origContent;
                btnSms.disabled = false;
            }
        }, 3500);
    }

    resetTheftAlarm() {
        this.stopBrowserSiren();
        const modal = document.getElementById('emergencyAlertModal');
        if (modal) modal.classList.remove('active');

        this._dispatchFirebaseCommand('reset_alarm', true);
        this._showToast('Alarm telah direset.', 'success');
    }

    // =========================================================================
    // METODE SISTEM INPUTAN PEMBATASAN JARAK / GEOFENCE RADIUS
    // =========================================================================
    _bindGeofenceControls() {
        // 1. Slider Radius
        const slider = document.getElementById('geofenceRadiusSlider');
        if (slider) {
            slider.addEventListener('input', (e) => {
                const val = parseInt(e.target.value);
                this.setGeofenceRadius(val, false);
            });
            slider.addEventListener('change', (e) => {
                const val = parseInt(e.target.value);
                this.setGeofenceRadius(val, true);
            });
        }

        // 2. Input Angka Langsung (Number Input)
        const numInput = document.getElementById('geofenceRadiusInput');
        if (numInput) {
            numInput.addEventListener('change', (e) => {
                let val = parseInt(e.target.value);
                if (isNaN(val) || val < 5) val = 5;
                if (val > 5000) val = 5000;
                this.setGeofenceRadius(val, true);
            });
        }

        // 3. Tombol-Tombol Preset Jarak (20m, 50m, 100m, 250m, 500m, 1000m)
        const presetBtns = document.querySelectorAll('.btn-preset');
        presetBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const rad = parseInt(btn.dataset.radius);
                if (rad) {
                    this.setGeofenceRadius(rad, true);
                    presetBtns.forEach(b => b.classList.remove('active'));
                    btn.classList.add('active');
                }
            });
        });

        // 4. Toggle Saklar Geofence Aktif / Nonaktif
        const toggleSwitch = document.getElementById('toggleGeofenceEnabled');
        if (toggleSwitch) {
            toggleSwitch.addEventListener('change', (e) => {
                this.setGeofenceEnabled(e.target.checked);
            });
        }

        // 5. Tombol Tetapkan Titik Parkir Saat Ini (Set Anchor Point)
        const btnAnchor = document.getElementById('btnSetAnchorPoint');
        if (btnAnchor) {
            btnAnchor.addEventListener('click', () => {
                const currentLat = this.mapController.lastLat;
                const currentLng = this.mapController.lastLng;
                if (currentLat && currentLng && Math.abs(currentLat) > 0.001) {
                    this.setAnchorPoint(currentLat, currentLng);
                    this.playChirp(2);
                    this._showToast(`Titik pusat geofence ditetapkan di: ${currentLat.toFixed(5)}, ${currentLng.toFixed(5)}`, 'success');
                } else {
                    this._showToast('Menunggu sinyal satelit GPS real dari kendaraan...', 'warning');
                }
            });
        }

        // 6. Checkbox Auto Cut-Off saat Melanggar Radius
        const chkCutoff = document.getElementById('chkAutoCutoffGeofence');
        if (chkCutoff) {
            chkCutoff.addEventListener('change', (e) => {
                this.autoCutoffGeofence = e.target.checked;
                this._dispatchFirebaseCommand('auto_cutoff_geofence', this.autoCutoffGeofence);
                this._showToast(`Auto Cut-off Keluar Radius: ${this.autoCutoffGeofence ? 'AKTIF' : 'NONAKTIF'}`, 'info');
            });
        }
    }

    setGeofenceRadius(radiusMeters, dispatchToCloud = true) {
        this.geofenceRadius = radiusMeters;

        // 1. Update UI Elements
        const slider = document.getElementById('geofenceRadiusSlider');
        const numInput = document.getElementById('geofenceRadiusInput');
        const displayLimit = document.getElementById('displayGeofenceLimit');

        if (numInput && parseInt(numInput.value) !== radiusMeters) {
            numInput.value = radiusMeters;
        }
        if (slider && parseInt(slider.value) !== Math.min(radiusMeters, 500)) {
            slider.value = Math.min(radiusMeters, 500);
        }
        if (displayLimit) {
            displayLimit.innerText = radiusMeters >= 1000 ? 
                `${(radiusMeters / 1000).toFixed(1)} km` : `${radiusMeters} m`;
        }

        // Highlight preset button yang cocok
        const presetBtns = document.querySelectorAll('.btn-preset');
        presetBtns.forEach(btn => {
            if (parseInt(btn.dataset.radius) === radiusMeters) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        });

        // 2. Update Lingkaran Visual di Peta Leaflet
        if (this.mapController) {
            this.mapController.setGeofenceRadius(radiusMeters);
        }

        // 3. Update Status Jarak Real-Time
        if (this.mapController && this.mapController.lastLat) {
            this.updateLiveDistance(this.mapController.lastLat, this.mapController.lastLng);
        }

        // 4. Kirim ke Firebase Cloud RTDB (Debounced)
        if (dispatchToCloud) {
            if (this._geofenceDebounceTimer) clearTimeout(this._geofenceDebounceTimer);
            this._geofenceDebounceTimer = setTimeout(() => {
                this._dispatchFirebaseCommand('geofence_radius', radiusMeters);
                this._showToast(`Batas radius aman disetel ke: ${radiusMeters} Meter`, 'info');
            }, 400);
        }
    }

    setGeofenceEnabled(enabled) {
        this.geofenceEnabled = enabled;

        const banner = document.getElementById('geofenceStatusBanner');
        const statusText = document.getElementById('geofenceStatusText');
        const toggle = document.getElementById('toggleGeofenceEnabled');
        
        if (toggle && toggle.checked !== enabled) {
            toggle.checked = enabled;
        }

        if (this.mapController) {
            this.mapController.toggleGeofence(enabled);
        }

        if (banner) {
            banner.classList.toggle('disabled', !enabled);
        }
        if (statusText) {
            statusText.innerText = enabled ? 'PAGAR VIRTUAL AKTIF (AMAN)' : 'PAGAR VIRTUAL DINONAKTIFKAN';
        }

        this._dispatchFirebaseCommand('geofence_enabled', enabled);
        this._showToast(`Pagar Virtual Geofence: ${enabled ? 'DIAKTIFKAN' : 'DINONAKTIFKAN'}`, enabled ? 'success' : 'warning');
    }

    setAnchorPoint(lat, lng) {
        this.anchorLat = lat;
        this.anchorLng = lng;

        if (this.mapController) {
            this.mapController.setGeofence(lat, lng, this.geofenceRadius);
        }

        this._dispatchFirebaseCommand('anchor_lat', lat);
        this._dispatchFirebaseCommand('anchor_lng', lng);
        this.updateLiveDistance(lat, lng);
    }

    calculateDistanceMeters(lat1, lon1, lat2, lon2) {
        if (!lat1 || !lon1 || !lat2 || !lon2) return 0;
        const R = 6371000; // Radius bumi dalam meter
        const dLat = (lat2 - lat1) * (Math.PI / 180);
        const dLon = (lon2 - lon1) * (Math.PI / 180);
        const a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
                  Math.cos(lat1 * (Math.PI / 180)) * Math.cos(lat2 * (Math.PI / 180)) *
                  Math.sin(dLon / 2) * Math.sin(dLon / 2);
        const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
        return R * c;
    }

    updateLiveDistance(currentLat, currentLng) {
        if (!currentLat || !currentLng || isNaN(currentLat) || isNaN(currentLng)) return;

        // Jika anchor belum pernah di-set, gunakan lokasi awal
        if (!this.anchorLat || !this.anchorLng) {
            this.anchorLat = currentLat;
            this.anchorLng = currentLng;
            if (this.mapController) {
                this.mapController.setGeofence(currentLat, currentLng, this.geofenceRadius);
            }
        }

        const dist = this.calculateDistanceMeters(this.anchorLat, this.anchorLng, currentLat, currentLng);
        const distEl = document.getElementById('liveGeofenceDistance');
        const banner = document.getElementById('geofenceStatusBanner');
        const icon = document.getElementById('geofenceStatusIcon');
        const statusText = document.getElementById('geofenceStatusText');
        const progBar = document.getElementById('geofenceProgressBar');

        if (distEl) {
            distEl.innerText = dist >= 1000 ? `${(dist / 1000).toFixed(2)} km` : `${dist.toFixed(1)} m`;
        }

        // Hitung persentase progress bar
        const percent = Math.min(100, (dist / this.geofenceRadius) * 100);
        if (progBar) {
            progBar.style.width = `${percent}%`;
        }

        // Cek Pelanggaran Batas Radius (Breach)
        if (this.geofenceEnabled && dist > this.geofenceRadius) {
            if (banner) banner.classList.add('breach');
            if (icon) icon.className = 'fa-solid fa-triangle-exclamation';
            if (statusText) statusText.innerHTML = '<span style="color: var(--accent-red);">⚠️ KELUAR DARI RADIUS AMAN!</span>';
            if (progBar) progBar.classList.add('danger');

            // Auto-Cutoff jika diaktifkan
            const btnEngine = document.getElementById('btnToggleEngine');
            const isCurrentlyLocked = btnEngine && btnEngine.dataset.locked === 'true';
            if (this.autoCutoffGeofence && !isCurrentlyLocked) {
                console.warn('[GEOFENCE AUTO-CUTOFF] Motor keluar radius! Mematikan mesin otomatis.');
                this.setEngineLock(true);
                this.triggerPanicAlarm();
            }
        } else {
            if (banner) banner.classList.remove('breach');
            if (icon) icon.className = 'fa-solid fa-shield-halved';
            if (statusText && this.geofenceEnabled) {
                statusText.innerText = 'PAGAR VIRTUAL AKTIF (AMAN)';
            }
            if (progBar) progBar.classList.remove('danger');
        }
    }

    _dispatchFirebaseCommand(commandKey, value) {
        if (window.appInstance && window.appInstance.isSimMode) {
            window.appInstance.handleSimulatedCommand(commandKey, value);
            return;
        }

        const vehicleId = window.currentVehicleId || APP_CONFIG.DEFAULT_VEHICLE_ID;
        const nowSec = Math.floor(Date.now() / 1000);
        const updates = {};
        updates[`vehicles/${vehicleId}/controls/${commandKey}`] = value;
        updates[`vehicles/${vehicleId}/controls/last_command_time`] = nowSec;

        // 1. Dispatch via Firebase Web SDK
        if (window.firebaseDb) {
            window.firebaseDb.ref().update(updates)
                .then(() => {
                    console.log(`[FIREBASE SDK] Perintah '${commandKey}: ${value}' sukses terkirim.`);
                })
                .catch(err => {
                    console.warn(`[FIREBASE SDK WARN] ${err.message}`);
                });
        }

        // 2. Dual-Dispatch via Direct HTTP REST API (Sangat Handal untuk Vercel / Mobile Browser)
        const restUrl = `${APP_CONFIG.FIREBASE_CONFIG.databaseURL}/vehicles/${vehicleId}/controls.json`;
        const restBody = JSON.stringify({
            [commandKey]: value,
            last_command_time: nowSec
        });

        fetch(restUrl, {
            method: 'PATCH',
            headers: { 'Content-Type': 'application/json' },
            body: restBody
        })
        .then(res => res.json())
        .then(data => {
            console.log(`[FIREBASE REST] Sukses terkirim ke Cloud Database:`, data);
        })
        .catch(err => {
            console.error(`[FIREBASE REST ERROR]`, err);
        });
    }

    _showConfirmationModal(title, message, onConfirm) {
        const modalHtml = `
            <div class="modal-overlay active" id="dynamicConfirmModal">
                <div class="emergency-modal-content">
                    <div class="alarm-icon-box" style="background: rgba(255, 171, 0, 0.2); border-color: var(--accent-amber); color: var(--accent-amber);">
                        <i class="fa-solid fa-triangle-exclamation"></i>
                    </div>
                    <h2 style="color: var(--accent-amber); font-size: 1.3rem;">${title}</h2>
                    <p style="color: var(--text-primary); margin-top: 12px; font-size: 0.95rem;">${message}</p>
                    <div class="modal-buttons" style="margin-top: 20px;">
                        <button class="btn-modal-cancel" id="btnCancelDynamicModal">Batal</button>
                        <button class="btn-modal-confirm" id="btnConfirmDynamicModal" style="background: var(--accent-red);">Ya, Eksekusi</button>
                    </div>
                </div>
            </div>
        `;

        const existingModal = document.getElementById('dynamicConfirmModal');
        if (existingModal) existingModal.remove();

        document.body.insertAdjacentHTML('beforeend', modalHtml);

        document.getElementById('btnCancelDynamicModal').addEventListener('click', () => {
            document.getElementById('dynamicConfirmModal').remove();
        });

        document.getElementById('btnConfirmDynamicModal').addEventListener('click', () => {
            document.getElementById('dynamicConfirmModal').remove();
            if (onConfirm) onConfirm();
        });
    }

    _showToast(message, type = 'info') {
        let toastContainer = document.getElementById('toastContainer');
        if (!toastContainer) {
            toastContainer = document.createElement('div');
            toastContainer.id = 'toastContainer';
            toastContainer.style.cssText = `
                position: fixed;
                top: 24px;
                right: 24px;
                z-index: 9999;
                display: flex;
                flex-direction: column;
                gap: 10px;
                pointer-events: none;
            `;
            document.body.appendChild(toastContainer);
        }

        const toast = document.createElement('div');
        const icon = type === 'error' ? 'fa-circle-exclamation' : (type === 'warning' ? 'fa-triangle-exclamation' : 'fa-circle-check');
        const color = type === 'error' ? 'var(--accent-red)' : (type === 'warning' ? 'var(--accent-amber)' : 'var(--accent-green)');

        toast.style.cssText = `
            background: rgba(17, 23, 38, 0.92);
            border: 1px solid ${color};
            border-left: 5px solid ${color};
            color: #fff;
            padding: 12px 18px;
            border-radius: 8px;
            font-family: 'Plus Jakarta Sans', sans-serif;
            font-size: 0.88rem;
            box-shadow: 0 8px 24px rgba(0,0,0,0.5);
            display: flex;
            align-items: center;
            gap: 10px;
            backdrop-filter: blur(8px);
            animation: slideInRight 0.3s cubic-bezier(0.16, 1, 0.3, 1);
            pointer-events: auto;
        `;

        toast.innerHTML = `<i class="fa-solid ${icon}" style="color: ${color}; font-size: 1.1rem;"></i> <span>${message}</span>`;
        toastContainer.appendChild(toast);

        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transform = 'translateX(50px)';
            toast.style.transition = 'all 0.3s ease';
            setTimeout(() => toast.remove(), 300);
        }, 3200);
    }
}
