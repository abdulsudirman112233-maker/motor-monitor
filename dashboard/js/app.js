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
            latitude: -5.460095,
            longitude: 122.616677,
            altitude: 40.0,
            speed: 0.0,
            heading: 0.0,
            satellites: 5,
            hdop: 1.2,
            gps_fixed: true,
            gsm_csq: 18,
            gsm_signal_percent: 60,
            gsm_network: "INDOSAT",
            battery_voltage: 12.6,
            power_source: "ACCU_12V",
            vibration_detected: false,
            engine_running: false,
            connection_mode: "WIFI_ONLINE",
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
        
        // 1. Inisialisasi Peta Leaflet
        try {
            this.mapController.init();
        } catch (mapErr) {
            console.error('[MAP INIT ERROR]', mapErr);
        }

        // 2. Inisialisasi Kontrol
        try {
            this.controls.init();
        } catch (ctrlErr) {
            console.error('[CONTROLS INIT ERROR]', ctrlErr);
        }

        // 3. Bind Event Topbar & Tombol Map
        this._bindTopBarControls();

        // 4. Render tampilan kartu telemetri awal
        this.telemetryViewer.updateTelemetryCards(this.telemetry, this.status);
        this.telemetryViewer.updateHeaderBadges(this.telemetry, this.status);

        // 5. Muat log awal
        this.telemetryViewer.appendLogEntry({
            event_type: "SYSTEM_READY",
            message: "Dashboard Pelacak GPS Real-Time Aktif & Terhubung ke Firebase Cloud.",
            datetime: new Date().toLocaleTimeString('id-ID'),
            latitude: this.telemetry.latitude,
            longitude: this.telemetry.longitude,
            speed: 0.0
        });

        // 6. Inisialisasi Koneksi Real-Time ke Firebase
        this._initFirebase();

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

        // Tombol Switch Tile Google Maps Resmi (Roadmap / Satelit Hybrid / Traffic / Dark)
        const btnGoogleRoad = document.getElementById('btnLayerGoogleRoadmap');
        if (btnGoogleRoad) {
            btnGoogleRoad.addEventListener('click', () => {
                this.mapController.switchLayer('googleRoadmap');
                this._setActiveMapButton(btnGoogleRoad);
                this.controls._showToast('Layer Peta: Google Maps', 'info');
            });
        }

        const btnGoogleSat = document.getElementById('btnLayerGoogleSatellite');
        if (btnGoogleSat) {
            btnGoogleSat.addEventListener('click', () => {
                this.mapController.switchLayer('googleSatellite');
                this._setActiveMapButton(btnGoogleSat);
                this.controls._showToast('Layer Peta: Google Satelit Hybrid', 'info');
            });
        }

        const btnGoogleTraffic = document.getElementById('btnLayerGoogleTraffic');
        if (btnGoogleTraffic) {
            btnGoogleTraffic.addEventListener('click', () => {
                this.mapController.switchLayer('googleTraffic');
                this._setActiveMapButton(btnGoogleTraffic);
                this.controls._showToast('Layer Peta: Google Maps Live Traffic', 'info');
            });
        }

        const btnDark = document.getElementById('btnLayerDark');
        if (btnDark) {
            btnDark.addEventListener('click', () => {
                this.mapController.switchLayer('dark');
                this._setActiveMapButton(btnDark);
                this.controls._showToast('Layer Peta: Cyberpunk Dark Mode', 'info');
            });
        }

        // Tombol Reset Jejak Trail
        const btnClear = document.getElementById('btnClearTrail');
        if (btnClear) {
            btnClear.addEventListener('click', () => {
                this.mapController.clearTrail();
                this.controls._showToast('Riwayat garis jejak rute telah dibersihkan', 'info');
            });
        }

        // Tombol Geofence Toggle
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

        // Dukungan Scroll Samping (Mouse Wheel / Touchpad) untuk Toolbar Peta di Laptop
        const mapActionsBar = document.querySelector('.map-actions');
        if (mapActionsBar) {
            mapActionsBar.addEventListener('wheel', (e) => {
                if (e.deltaY !== 0) {
                    e.preventDefault();
                    mapActionsBar.scrollLeft += e.deltaY;
                }
            }, { passive: false });
        }

        // Tombol Buka di Google Maps Real
        const btnOpenMaps = document.getElementById('btnOpenGoogleMaps');
        if (btnOpenMaps) {
            btnOpenMaps.addEventListener('click', () => {
                const lat = this.mapController.lastLat || this.telemetry.latitude;
                const lng = this.mapController.lastLng || this.telemetry.longitude;
                if (lat && lng) {
                    const mapsUrl = `https://www.google.com/maps?q=${lat},${lng}`;
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
                const firebaseApp = (firebase.apps && firebase.apps.length > 0) ? 
                    firebase.app() : 
                    firebase.initializeApp(APP_CONFIG.FIREBASE_CONFIG);
                
                // Inisialisasi Firebase Analytics jika tersedia
                if (typeof firebase.analytics === 'function' && APP_CONFIG.FIREBASE_CONFIG.measurementId) {
                    try {
                        window.firebaseAnalytics = firebase.analytics();
                        console.log('[FIREBASE] Analytics initialized.');
                    } catch (analyticsErr) {
                        console.log('[FIREBASE] Analytics notice:', analyticsErr.message);
                    }
                }

                window.firebaseDb = firebase.database();
                const vehicleRef = window.firebaseDb.ref(`vehicles/${this.vehicleId}`);
                
                // 1. Listener Telemetri Real-Time dari Perangkat Keras
                vehicleRef.child('telemetry').on('value', snapshot => {
                    const raw = snapshot.val();
                    if (!raw) return;
                    
                    let data = raw;
                    // Jika Firebase menyimpan sebagai list push object, ambil item paling terakhir
                    if (!raw.latitude && typeof raw === 'object') {
                        const keys = Object.keys(raw);
                        if (keys.length > 0) {
                            data = raw[keys[keys.length - 1]];
                        }
                    }

                    if (data && data.latitude && data.longitude) {
                        this.telemetry = data;
                        
                        // Perbarui Peta & Speedometer dengan data real
                        this.mapController.updatePosition(
                            data.latitude,
                            data.longitude,
                            data.heading || 0,
                            data.speed || 0,
                            this.status.alarm_active
                        );
                        
                        // Perbarui Jarak Real-Time Geofence
                        this.controls.updateLiveDistance(data.latitude, data.longitude);

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

                // 3. Listener Kontrol & Pembatasan Jarak Geofence Real-Time
                vehicleRef.child('controls').on('value', snapshot => {
                    const ctrlData = snapshot.val();
                    if (ctrlData) {
                        if (ctrlData.geofence_radius && ctrlData.geofence_radius !== this.controls.geofenceRadius) {
                            this.controls.setGeofenceRadius(ctrlData.geofence_radius, false);
                        }
                        if (ctrlData.geofence_enabled !== undefined && ctrlData.geofence_enabled !== this.controls.geofenceEnabled) {
                            this.controls.setGeofenceEnabled(ctrlData.geofence_enabled);
                        }
                        if (ctrlData.anchor_lat && ctrlData.anchor_lng) {
                            this.controls.anchorLat = ctrlData.anchor_lat;
                            this.controls.anchorLng = ctrlData.anchor_lng;
                            this.mapController.setGeofence(ctrlData.anchor_lat, ctrlData.anchor_lng, this.controls.geofenceRadius);
                        }
                    }
                });

                // 4. Listener Riwayat Log Real-Time
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

// Inisialisasi Otomatis & Universal (Mendukung Direct Load, Deferred, dan Vercel CDN)
function startApp() {
    if (window._appStarted) return;
    window._appStarted = true;
    const app = new App();
    app.init();
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', startApp);
} else {
    // Jika DOM sudah selesai dimuat (misal pada script deferred / cached), langsung jalankan
    startApp();
}
