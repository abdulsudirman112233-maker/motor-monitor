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
        DEFAULT_CENTER: [-6.2088, 106.8456], // Posisi inisial sementara sebelum GPS real terdeteksi
        DEFAULT_ZOOM: 17,
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
    },

    // Rute Waypoints untuk Mode Simulasi Live Demo
    SIMULATION_ROUTE: [
        { lat: -6.2088, lng: 106.8456, speed: 0.0, heading: 45 },
        { lat: -6.2085, lng: 106.8462, speed: 22.4, heading: 60 },
        { lat: -6.2081, lng: 106.8471, speed: 41.8, heading: 65 },
        { lat: -6.2075, lng: 106.8483, speed: 53.2, heading: 62 },
        { lat: -6.2067, lng: 106.8496, speed: 48.0, heading: 55 },
        { lat: -6.2058, lng: 106.8505, speed: 36.5, heading: 40 },
        { lat: -6.2048, lng: 106.8510, speed: 20.0, heading: 15 },
        { lat: -6.2039, lng: 106.8512, speed: 0.0, heading: 0 },
        { lat: -6.2032, lng: 106.8515, speed: 18.2, heading: 350 },
        { lat: -6.2021, lng: 106.8519, speed: 45.0, heading: 345 },
        { lat: -6.2008, lng: 106.8524, speed: 58.7, heading: 340 }
    ]
};
