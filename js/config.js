// =============================================================================
// KONFIGURASI WEB DASHBOARD & FIREBASE CLIENT
// =============================================================================

const APP_CONFIG = {
    // ID Kendaraan Default di Firebase Realtime Database
    DEFAULT_VEHICLE_ID: "vehicle_01",

    // Konfigurasi Firebase Web SDK (motor-monitor-9f391)
    FIREBASE_CONFIG: {
        apiKey: "AIzaSyCrzjFytXDgX2QQMfvq64rvltuDJPjlY-0",
        authDomain: "motor-monitor-9f391.firebaseapp.com",
        databaseURL: "https://motor-monitor-9f391-default-rtdb.asia-southeast1.firebasedatabase.app",
        projectId: "motor-monitor-9f391",
        storageBucket: "motor-monitor-9f391.firebasestorage.app",
        messagingSenderId: "103315159286",
        appId: "1:103315159286:web:054d019ec3f41b8ce9d4d7",
        measurementId: "G-4JD8NJB5TT"
    },

    // Parameter Peta Leaflet & Geofence (Real Map & Live Tracking)
    MAP: {
        DEFAULT_CENTER: [-5.460095, 122.616677], // Titik GPS Real Motor (Baubau, Sulawesi Tenggara)
        DEFAULT_ZOOM: 18,
        DEFAULT_GEOFENCE_RADIUS: 75, // Radius Pagar Virtual (75 Meter)
        MAX_POLYLINE_POINTS: 500,
        TILE_LAYER_DARK: "https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png",
        TILE_LAYER_STREET: "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
        TILE_LAYER_SATELLITE: "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"
    },

    // Pengaturan Audio Siren Web Audio API
    AUDIO: {
        SIREN_HIGH_FREQ: 880, // Hz
        SIREN_LOW_FREQ: 440,  // Hz
        BEEP_FREQ: 1200       // Hz
    }
};
