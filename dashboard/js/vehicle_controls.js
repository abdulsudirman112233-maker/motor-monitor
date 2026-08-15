// =============================================================================
// VEHICLE CONTROLS & COMMAND DISPATCHER
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
    }

    init() {
        this._bindUIEvents();
        this._initAudio();
        console.log('[CONTROLS] Vehicle Controls Initialized.');
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
                gain.gain.setValueAtTime(0.15, this.audioContext.currentTime);
                gain.gain.exponentialRampToValueAtTime(0.01, this.audioContext.currentTime + 0.12);

                osc.connect(gain);
                gain.connect(this.audioContext.destination);
                osc.start();
                osc.stop(this.audioContext.currentTime + 0.12);
            }, i * 160);
        }
    }

    startBrowserSiren() {
        if (!this.audioContext || this.isSirenPlaying) return;
        if (this.audioContext.state === 'suspended') {
            this.audioContext.resume();
        }

        this.isSirenPlaying = true;
        const osc = this.audioContext.createOscillator();
        const gain = this.audioContext.createGain();
        osc.type = 'sawtooth';
        
        // Modulasi nada sirene bergantian tinggi-rendah
        const now = this.audioContext.currentTime;
        osc.frequency.setValueAtTime(APP_CONFIG.AUDIO.SIREN_HIGH_FREQ, now);
        
        gain.gain.setValueAtTime(0.2, now);
        osc.connect(gain);
        gain.connect(this.audioContext.destination);
        osc.start();
        this.sirenOscillator = { osc, gain };

        // Animasi frekuensi LFO
        let high = true;
        this.sirenInterval = setInterval(() => {
            if (!this.isSirenPlaying || !this.sirenOscillator) return;
            const t = this.audioContext.currentTime;
            high = !high;
            this.sirenOscillator.osc.frequency.setTargetAtTime(
                high ? APP_CONFIG.AUDIO.SIREN_HIGH_FREQ : APP_CONFIG.AUDIO.SIREN_LOW_FREQ,
                t, 0.08
            );
        }, 300);
    }

    stopBrowserSiren() {
        this.isSirenPlaying = false;
        if (this.sirenInterval) clearInterval(this.sirenInterval);
        if (this.sirenOscillator) {
            try {
                this.sirenOscillator.gain.gain.setTargetAtTime(0.001, this.audioContext.currentTime, 0.05);
                setTimeout(() => {
                    this.sirenOscillator.osc.stop();
                    this.sirenOscillator = null;
                }, 100);
            } catch (e) {}
        }
    }

    _bindUIEvents() {
        // 1. Engine Kill Switch Button
        const btnEngine = document.getElementById('btnToggleEngine');
        if (btnEngine) {
            btnEngine.addEventListener('click', () => {
                const isCurrentlyLocked = btnEngine.dataset.locked === 'true';
                if (!isCurrentlyLocked) {
                    // Tampilkan dialog konfirmasi matikan mesin
                    this._showConfirmationModal(
                        'Matikan Mesin Kendaraan?',
                        'Relay pengapian akan memutus arus CDI/Starter. Kendaraan tidak akan bisa dinyalakan sampai dibuka kembali.',
                        () => this.setEngineLock(true)
                    );
                } else {
                    this.setEngineLock(false);
                }
            });
        }

        // 2. Tombol Arm / Disarm
        const btnArm = document.getElementById('btnToggleArm');
        if (btnArm) {
            btnArm.addEventListener('click', () => {
                const isCurrentlyArmed = btnArm.dataset.armed === 'true';
                this.setArmSystem(!isCurrentlyArmed);
            });
        }

        // 3. Tombol Panic Siren
        const btnPanic = document.getElementById('btnPanicSiren');
        if (btnPanic) {
            btnPanic.addEventListener('click', () => {
                this.triggerPanicAlarm();
            });
        }

        // 4. Tombol Cari Kendaraan (Locator Chirp)
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
            btnEngine.textContent = lockState ? 'RESTORE MESIN' : 'MATIKAN MESIN';
            btnEngine.className = lockState ? 'btn-engine-toggle unlocked-state' : 'btn-engine-toggle';
        }
        if (boxEngine) {
            if (lockState) boxEngine.classList.add('locked');
            else boxEngine.classList.remove('locked');
        }

        this._dispatchFirebaseCommand('lock_engine', lockState);
        this._showToast(lockState ? 'Perintah: Mesin Telah Dimatikan (Cut-Off)' : 'Perintah: Pengapian Mesin Dipulihkan', lockState ? 'error' : 'success');
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
        this._dispatchFirebaseCommand('emergency_sms_request', true);
        this._showToast('Permintaan Kirim SMS Darurat Terkirim ke Modul SIM800L', 'warning');
    }

    resetTheftAlarm() {
        this.stopBrowserSiren();
        const modal = document.getElementById('emergencyAlertModal');
        if (modal) modal.classList.remove('active');

        this._dispatchFirebaseCommand('reset_alarm', true);
        this._showToast('Alarm telah direset.', 'success');
    }

    _dispatchFirebaseCommand(commandKey, value) {
        if (window.appInstance && window.appInstance.isSimMode) {
            // Mode Simulasi: Simulasikan respons instan
            window.appInstance.handleSimulatedCommand(commandKey, value);
            return;
        }

        // Tembak Firebase RTDB jika online
        if (window.firebaseDb && window.currentVehicleId) {
            const updates = {};
            updates[`vehicles/${window.currentVehicleId}/controls/${commandKey}`] = value;
            updates[`vehicles/${window.currentVehicleId}/controls/last_command_time`] = Math.floor(Date.now() / 1000);
            
            window.firebaseDb.ref().update(updates)
                .then(() => {
                    console.log(`[FIREBASE] Perintah '${commandKey}: ${value}' sukses terkirim.`);
                })
                .catch(err => {
                    console.error(`[FIREBASE] Gagal kirim perintah:`, err);
                    this._showToast(`Gagal kirim perintah: ${err.message}`, 'error');
                });
        }
    }

    _showConfirmationModal(title, message, onConfirm) {
        const modalHtml = `
            <div class="modal-overlay active" id="dynamicConfirmModal">
                <div class="emergency-modal-content">
                    <div class="alarm-icon-box" style="background: rgba(255, 171, 0, 0.2); border-color: var(--accent-amber); color: var(--accent-amber);">
                        <i class="fa-solid fa-triangle-exclamation"></i>
                    </div>
                    <h2>${title}</h2>
                    <p style="color: var(--text-secondary); margin-top: 10px; font-size: 0.88rem;">${message}</p>
                    <div class="modal-buttons">
                        <button class="btn-modal-cancel" id="btnCancelConfirm">Batal</button>
                        <button class="btn-modal-confirm" id="btnOkConfirm">Ya, Lanjutkan</button>
                    </div>
                </div>
            </div>
        `;
        const div = document.createElement('div');
        div.innerHTML = modalHtml;
        document.body.appendChild(div);

        const modalEl = document.getElementById('dynamicConfirmModal');
        document.getElementById('btnCancelConfirm').onclick = () => {
            modalEl.remove();
        };
        document.getElementById('btnOkConfirm').onclick = () => {
            modalEl.remove();
            onConfirm();
        };
    }

    _showToast(message, type = 'info') {
        let container = document.getElementById('toastContainer');
        if (!container) {
            container = document.createElement('div');
            container.id = 'toastContainer';
            container.className = 'toast-container';
            document.body.appendChild(container);
        }

        const toast = document.createElement('div');
        toast.className = `toast-item ${type}`;
        
        let icon = 'fa-info-circle';
        if (type === 'error') icon = 'fa-triangle-exclamation';
        if (type === 'success') icon = 'fa-circle-check';
        if (type === 'warning') icon = 'fa-bell';

        toast.innerHTML = `
            <i class="fa-solid ${icon}"></i>
            <span>${message}</span>
        `;
        container.appendChild(toast);

        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transform = 'translateX(100%)';
            toast.style.transition = 'all 0.3s ease';
            setTimeout(() => toast.remove(), 300);
        }, 3500);
    }
}
