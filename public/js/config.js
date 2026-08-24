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

    // Parameter Peta Google Maps & Geofence (Real Map & Live Tracking)
    MAP: {
        DEFAULT_CENTER: [-5.460095, 122.616677], // Titik GPS Real Motor (Baubau, Sulawesi Tenggara)
        DEFAULT_ZOOM: 18,
        DEFAULT_GEOFENCE_RADIUS: 20, // Radius Pagar Virtual (20 Meter)
        MAX_POLYLINE_POINTS: 500,

        // URL Google Maps Tiles Resmi (Roadmap, Satellite Hybrid, Traffic, Dark)
        TILE_LAYER_GOOGLE_ROADMAP: "https://mt{s}.google.com/vt/lyrs=m&hl=id&gl=ID&x={x}&y={y}&z={z}",
        TILE_LAYER_GOOGLE_SATELLITE: "https://mt{s}.google.com/vt/lyrs=y&hl=id&gl=ID&x={x}&y={y}&z={z}", // Satelit + Label Jalan
        TILE_LAYER_GOOGLE_TRAFFIC: "https://mt{s}.google.com/vt/lyrs=m,traffic&hl=id&gl=ID&x={x}&y={y}&z={z}", // Peta Jalan + Live Traffic
        TILE_LAYER_DARK: "https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
    },

    // Pengaturan Audio Siren Web Audio API
    AUDIO: {
        SIREN_HIGH_FREQ: 880, // Hz
        SIREN_LOW_FREQ: 440,  // Hz
        BEEP_FREQ: 1200       // Hz
    }
};
