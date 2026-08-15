// =============================================================================
// ENGINE PETA LEAFLET.JS - PURE REAL-TIME GPS TRACKING (NO DUMMY DATA)
// =============================================================================

class MapController {
    constructor(elementId) {
        this.elementId = elementId;
        this.map = null;
        this.vehicleMarker = null;
        this.trailPolyline = null;
        this.geofenceCircle = null;
        this.currentLayer = 'street';
        this.tileLayers = {};
        this.pathCoordinates = [];
        this.autoCenter = true;
        this.lastLat = null;
        this.lastLng = null;
    }

    init() {
        // Inisialisasi Map Leaflet dengan tampilan umum Indonesia sebelum titik GPS real terdeteksi
        this.map = L.map(this.elementId, {
            center: [-2.5489, 118.0149], // Pusat Indonesia
            zoom: 5,
            zoomControl: false,
            attributionControl: false
        });

        // Posisi Zoom Control di kanan bawah
        L.control.zoom({ position: 'bottomright' }).addTo(this.map);

        // Siapkan Tile Layers (Peta Jalanan Real, Satelit Asli, Dark Mode)
        this.tileLayers.street = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_STREET, {
            maxZoom: 19,
            attribution: '&copy; OpenStreetMap'
        });

        this.tileLayers.dark = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_DARK, {
            maxZoom: 19,
            subdomains: 'abcd'
        });

        this.tileLayers.satellite = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_SATELLITE, {
            maxZoom: 19
        });

        // Set layer default ke Peta Jalanan Real
        this.tileLayers.street.addTo(this.map);

        // Inisialisasi garis jejak rute (Polyline)
        this.trailPolyline = L.polyline([], {
            color: '#00f0ff',
            weight: 4,
            opacity: 0.85,
            smoothFactor: 1,
            lineJoin: 'round',
            dashArray: '2, 6'
        }).addTo(this.map);

        // Event saat user menggeser peta manual (matikan auto-center sementara)
        this.map.on('dragstart', () => {
            this.autoCenter = false;
            const btnCenter = document.getElementById('btnAutoCenter');
            if (btnCenter) btnCenter.classList.remove('active');
        });

        console.log('[MAP] Pure Real GPS Leaflet Map Initialized.');
    }

    _createVehicleMarker(latLng) {
        const customIcon = L.divIcon({
            className: 'vehicle-marker-icon',
            html: `
                <div class="marker-pin" id="markerPinElement">
                    <i class="fa-solid fa-motorcycle" id="markerIconElement"></i>
                </div>
            `,
            iconSize: [44, 44],
            iconAnchor: [22, 22]
        });

        this.vehicleMarker = L.marker(latLng, { icon: customIcon }).addTo(this.map);

        // Buat Geofence Circle mengikuti posisi awal motor
        if (!this.geofenceCircle) {
            this.geofenceCircle = L.circle(latLng, {
                color: '#7928ca',
                fillColor: '#7928ca',
                fillOpacity: 0.12,
                weight: 2,
                dashArray: '6, 6',
                radius: 250
            }).addTo(this.map);
        }
    }

    updatePosition(lat, lng, heading = 0, speed = 0, isAlarmActive = false) {
        const numLat = parseFloat(lat);
        const numLng = parseFloat(lng);

        // Validasi koordinat real: Abaikan jika bernilai 0.00000 (GPS sedang mencari satelit)
        if (!numLat || !numLng || isNaN(numLat) || isNaN(numLng) || (Math.abs(numLat) < 0.001 && Math.abs(numLng) < 0.001)) {
            const addressEl = document.getElementById('addressDisplay');
            if (addressEl) {
                addressEl.innerHTML = '<span style="color: var(--accent-orange);"><i class="fa-solid fa-satellite-dish fa-spin"></i> Menunggu sinyal satelit GPS real dari motor...</span>';
            }
            return;
        }

        const newLatLng = [numLat, numLng];
        const isFirstFix = (this.lastLat === null || this.lastLng === null);
        this.lastLat = numLat;
        this.lastLng = numLng;

        // 1. Buat Marker pertama kali jika belum ada
        if (!this.vehicleMarker) {
            this._createVehicleMarker(newLatLng);
        } else {
            this.vehicleMarker.setLatLng(newLatLng);
        }

        // 2. Perbarui Tooltip / Popup Info Real-Time pada Marker
        const popupContent = `
            <div style="font-family: 'Plus Jakarta Sans', sans-serif; font-size: 12px; color: #fff; padding: 4px; min-width: 170px;">
                <div style="font-weight: 700; color: #00f0ff; margin-bottom: 6px; font-size: 13px;">
                    <i class="fa-solid fa-motorcycle"></i> ${APP_CONFIG.DEFAULT_VEHICLE_ID.toUpperCase()} (ONLINE)
                </div>
                <div style="margin: 2px 0;"><b>Kecepatan:</b> ${speed.toFixed(1)} km/h</div>
                <div style="margin: 2px 0;"><b>Latitude:</b> ${numLat.toFixed(6)}</div>
                <div style="margin: 2px 0;"><b>Longitude:</b> ${numLng.toFixed(6)}</div>
                <div style="margin-top: 8px;">
                    <a href="https://www.google.com/maps?q=${numLat},${numLng}" target="_blank" style="color: #00e676; font-weight: 600; text-decoration: underline;">
                        <i class="fa-solid fa-arrow-up-right-from-square"></i> Buka di Google Maps
                    </a>
                </div>
            </div>
        `;
        this.vehicleMarker.bindPopup(popupContent);

        // 3. Putar Ikon Motor sesuai Heading/Arah Kompas Real
        const iconEl = document.getElementById('markerIconElement');
        const pinEl = document.getElementById('markerPinElement');
        if (iconEl) {
            iconEl.style.transform = `rotate(${heading}deg)`;
            iconEl.style.transition = 'transform 0.4s ease';
        }

        if (pinEl) {
            if (isAlarmActive) {
                pinEl.classList.add('alarm-active');
            } else {
                pinEl.classList.remove('alarm-active');
            }
        }

        // 4. Tambahkan ke jejak lintasan (Polyline)
        this.pathCoordinates.push(newLatLng);
        if (this.pathCoordinates.length > APP_CONFIG.MAP.MAX_POLYLINE_POINTS) {
            this.pathCoordinates.shift();
        }
        this.trailPolyline.setLatLngs(this.pathCoordinates);

        // 5. Auto Center kamera ke titik GPS nyata
        if (isFirstFix) {
            this.map.flyTo(newLatLng, 18, { duration: 1.5 });
        } else if (this.autoCenter) {
            this.map.panTo(newLatLng, { animate: true, duration: 0.5 });
        }

        // 6. Lakukan reverse geocoding untuk menampilkan nama jalan & kota nyata
        this._reverseGeocode(numLat, numLng);
    }

    setGeofence(centerLat, centerLng, radiusMeters) {
        if (this.geofenceCircle) {
            this.geofenceCircle.setLatLng([centerLat, centerLng]);
            this.geofenceCircle.setRadius(radiusMeters);
        }
    }

    toggleGeofence(visible) {
        if (!this.geofenceCircle) return;
        if (visible) {
            this.geofenceCircle.addTo(this.map);
        } else {
            this.map.removeLayer(this.geofenceCircle);
        }
    }

    centerOnVehicle() {
        if (this.lastLat && this.lastLng) {
            this.autoCenter = true;
            this.map.flyTo([this.lastLat, this.lastLng], 18, { duration: 1 });
            const btnCenter = document.getElementById('btnAutoCenter');
            if (btnCenter) btnCenter.classList.add('active');
        }
    }

    switchLayer(layerName) {
        if (this.tileLayers[this.currentLayer]) {
            this.map.removeLayer(this.tileLayers[this.currentLayer]);
        }
        if (this.tileLayers[layerName]) {
            this.tileLayers[layerName].addTo(this.map);
            this.currentLayer = layerName;
        }
    }

    clearTrail() {
        this.pathCoordinates = [];
        this.trailPolyline.setLatLngs([]);
    }

    _reverseGeocode(lat, lng) {
        const now = Date.now();
        if (this._lastGeocodeTime && now - this._lastGeocodeTime < 8000) return;
        this._lastGeocodeTime = now;

        const addressEl = document.getElementById('addressDisplay');
        if (!addressEl) return;

        fetch(`https://nominatim.openstreetmap.org/reverse?format=json&lat=${lat}&lon=${lng}&zoom=18&addressdetails=1`)
            .then(res => res.json())
            .then(data => {
                if (data && data.display_name) {
                    addressEl.innerHTML = `<i class="fa-solid fa-location-dot" style="color: var(--accent-green);"></i> <span>${data.display_name}</span>`;
                    addressEl.title = data.display_name;
                }
            })
            .catch(() => {
                addressEl.innerHTML = `<i class="fa-solid fa-location-dot" style="color: var(--primary);"></i> <span>Titik Koordinat: ${lat.toFixed(6)}, ${lng.toFixed(6)}</span>`;
            });
    }
}
