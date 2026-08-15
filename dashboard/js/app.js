// =============================================================================
// MAIN APPLICATION CONTROLLER - REAL-TIME GPS TRACKING & FIREBASE SYNC
// =============================================================================

class App {
    constructor() {
        this.vehicleId = APP_CONFIG.DEFAULT_VEHICLE_ID;
        this.mapController = new MapController('map');
        this.telemetryViewer = new TelemetryViewer();
        this.controls = new VehicleControls(null, this.telemetryViewer, this.mapController);
        
        this.telemetry = {
            latitude: 0.0,
            longitude: 0.0,
            altitude: 0.0,
            speed: 0.0,
            heading: 0.0,
            satellites: 0,
            hdop: 99.0,
            gps_fixed: false,
            gsm_csq: 0,
            gsm_signal_percent: 0,
            gsm_network: "CONNECTING...",
            battery_voltage: 12.6,
            power_source: "ACCU_12V",
            vibration_detected: false,
            engine_running: false,
            connection_mode: "CONNECTING...",
            timestamp: Math.floor(Date.now() / 1000)
        };

        this.status = {
            armed: true,
            alarm_active: false,
            engine_locked: false,
            theft_alert: false,
            last_alarm_reason: "NONE"
        };
    }

    init() {
        console.log('[APP] Inisialisasi Real-Time GPS Vehicle Security Dashboard...');
        
        // 1. Inisialisasi Peta
        this.mapController.init();

        // 2. Inisialisasi Kontrol
        this.controls.init();

        // 3. Bind Event Topbar & Tombol Map
        this._bindTopBarControls();

        // 4. Inisialisasi Koneksi Real-Time ke Firebase
        this._initFirebase();

        // 5. Muat log awal
        this.telemetryViewer.appendLogEntry({
            event_type: "SYSTEM_READY",
            message: "Dashboard Pelacak GPS Real-Time Aktif & Terhubung ke Firebase.",
            datetime: new Date().toLocaleTimeString('id-ID'),
            latitude: this.telemetry.latitude,
            longitude: this.telemetry.longitude,
            speed: 0.0
        });

        // Set global reference untuk debugging
        window.appInstance = this;
        window.currentVehicleId = this.vehicleId;
    }

    _bindTopBarControls() {
        // Tombol Auto Center
        const btnCenter = document.getElementById('btnAutoCenter');
        if (btnCenter) {
            btnCenter.addEventListener('click', () => {
                this.mapController.centerOnVehicle();
            });
        }

        // Tombol Switch Tile Map (Dark / Street / Satelit Real)
        const btnDarkLayer = document.getElementById('btnLayerDark');
        const btnStreetLayer = document.getElementById('btnLayerStreet');
        const btnSatLayer = document.getElementById('btnLayerSatellite');

        if (btnDarkLayer) {
            btnDarkLayer.addEventListener('click', () => {
                this._setActiveMapButton(btnDarkLayer);
                this.mapController.switchLayer('dark');
            });
        }
        if (btnStreetLayer) {
            btnStreetLayer.addEventListener('click', () => {
                this._setActiveMapButton(btnStreetLayer);
                this.mapController.switchLayer('street');
            });
        }
        if (btnSatLayer) {
            btnSatLayer.addEventListener('click', () => {
                this._setActiveMapButton(btnSatLayer);
                this.mapController.switchLayer('satellite');
            });
        }

        // Tombol Clear Jejak (Trail)
        const btnClearTrail = document.getElementById('btnClearTrail');
        if (btnClearTrail) {
            btnClearTrail.addEventListener('click', () => {
                this.mapController.clearTrail();
                this.controls._showToast('Jejak lintasan rute telah dibersihkan', 'info');
            });
        }

        // Tombol Toggle Geofence
        const btnToggleGeofence = document.getElementById('btnToggleGeofence');
        if (btnToggleGeofence) {
            let geofenceActive = true;
            btnToggleGeofence.addEventListener('click', () => {
                geofenceActive = !geofenceActive;
                this.mapController.toggleGeofence(geofenceActive);
                btnToggleGeofence.classList.toggle('active', geofenceActive);
                this.controls._showToast(geofenceActive ? 'Geofence ditampilkan' : 'Geofence disembunyikan', 'info');
            });
        }

        // Tombol Buka di Google Maps Real
        const btnOpenMaps = document.getElementById('btnOpenGoogleMaps');
        if (btnOpenMaps) {
            btnOpenMaps.addEventListener('click', () => {
                if (this.mapController.lastLat && this.mapController.lastLng) {
                    const mapsUrl = `https://www.google.com/maps?q=${this.mapController.lastLat},${this.mapController.lastLng}`;
                    window.open(mapsUrl, '_blank');
                } else {
                    this.controls._showToast('Menunggu koordinat GPS real dari kendaraan...', 'warning');
                }
            });
        }
    }

    _setActiveMapButton(activeBtn) {
        document.querySelectorAll('.btn-map-layer').forEach(btn => btn.classList.remove('active'));
        activeBtn.classList.add('active');
    }

    _initFirebase() {
        try {
            if (typeof firebase !== 'undefined' && APP_CONFIG.FIREBASE_CONFIG.apiKey) {
                const firebaseApp = firebase.initializeApp(APP_CONFIG.FIREBASE_CONFIG);
                
                // Inisialisasi Firebase Analytics jika tersedia
                if (typeof firebase.analytics === 'function' && APP_CONFIG.FIREBASE_CONFIG.measurementId) {
                    try {
                        window.firebaseAnalytics = firebase.analytics();
                        console.log('[FIREBASE] Analytics initialized successfully.');
                    } catch (analyticsErr) {
                        console.log('[FIREBASE] Analytics notice:', analyticsErr.message);
                    }
                }

                window.firebaseDb = firebase.database();
                const vehicleRef = window.firebaseDb.ref(`vehicles/${this.vehicleId}`);
                
                // 1. Listener Telemetri Real-Time dari Perangkat Keras
                vehicleRef.child('telemetry').on('value', snapshot => {
                    const data = snapshot.val();
                    if (data) {
                        this.telemetry = data;
                        
                        // Perbarui Peta & Speedometer dengan data real
                        this.mapController.updatePosition(
                            data.latitude,
                            data.longitude,
                            data.heading || 0,
                            data.speed || 0,
                            this.status.alarm_active
                        );
                        
                        this.telemetryViewer.updateTelemetryCards(data, this.status);
                    }
                });

                // 2. Listener Status Keamanan Real-Time
                vehicleRef.child('status').on('value', snapshot => {
                    const statusData = snapshot.val();
                    if (statusData) {
                        this.status = statusData;
                        this.telemetryViewer.updateHeaderBadges(this.telemetry, statusData);
                        
                        if (statusData.theft_alert || statusData.alarm_active) {
                            this.controls.triggerPanicAlarm();
                        }
                    }
                });

                // 3. Listener Riwayat Log Real-Time
                vehicleRef.child('logs').limitToLast(20).on('child_added', snapshot => {
                    const logData = snapshot.val();
                    if (logData) {
                        this.telemetryViewer.appendLogEntry(logData);
                    }
                });

                console.log('[FIREBASE] Real-Time Live Tracking Aktif untuk project: ' + APP_CONFIG.FIREBASE_CONFIG.projectId);
            }
        } catch (e) {
            console.error('[FIREBASE] Gagal terhubung ke Firebase:', e);
        }
    }
}

// Inisialisasi saat Halaman Selesai Dimuat
document.addEventListener('DOMContentLoaded', () => {
    const app = new App();
    app.init();
});
