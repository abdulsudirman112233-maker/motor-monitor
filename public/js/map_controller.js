// =============================================================================
// ENGINE PETA LEAFLET.JS - MARKER KENDARAAN, HEADING, TRAIL & GEOFENCE
// =============================================================================

class MapController {
    constructor(elementId) {
        this.elementId = elementId;
        this.map = null;
        this.vehicleMarker = null;
        this.trailPolyline = null;
        this.geofenceCircle = null;
        this.currentLayer = 'dark';
        this.tileLayers = {};
        this.pathCoordinates = [];
        this.autoCenter = true;
        this.lastLat = null;
        this.lastLng = null;
    }

    init() {
        // Inisialisasi Map Leaflet
        this.map = L.map(this.elementId, {
            center: APP_CONFIG.MAP.DEFAULT_CENTER,
            zoom: APP_CONFIG.MAP.DEFAULT_ZOOM,
            zoomControl: false,
            attributionControl: false
        });

        // Posisi Zoom Control di kanan bawah
        L.control.zoom({ position: 'bottomright' }).addTo(this.map);

        // Siapkan Tile Layers
        this.tileLayers.dark = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_DARK, {
            maxZoom: 19,
            subdomains: 'abcd'
        });

        this.tileLayers.street = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_STREET, {
            maxZoom: 19
        });

        this.tileLayers.satellite = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_SATELLITE, {
            maxZoom: 19
        });

        // Set layer default
        this.tileLayers.dark.addTo(this.map);

        // Buat custom marker kendaraan
        this._createVehicleMarker(APP_CONFIG.MAP.DEFAULT_CENTER);

        // Inisialisasi garis jejak rute (Polyline)
        this.trailPolyline = L.polyline([], {
            color: '#00f0ff',
            weight: 4,
            opacity: 0.85,
            smoothFactor: 1,
            lineJoin: 'round',
            dashArray: '2, 6'
        }).addTo(this.map);

        // Inisialisasi Geofence Circle
        this.geofenceCircle = L.circle(APP_CONFIG.MAP.DEFAULT_CENTER, {
            color: '#7928ca',
            fillColor: '#7928ca',
            fillOpacity: 0.12,
            weight: 2,
            dashArray: '6, 6',
            radius: 250
        }).addTo(this.map);

        // Event saat user menggeser peta manual (matikan auto-center sementara)
        this.map.on('dragstart', () => {
            this.autoCenter = false;
            const btnCenter = document.getElementById('btnAutoCenter');
            if (btnCenter) btnCenter.classList.remove('active');
        });

        console.log('[MAP] Leaflet Map Initialized Successfully.');
    }

    _createVehicleMarker(latLng) {
        const customIcon = L.divIcon({
            className: 'vehicle-marker-icon',
            html: `
                <div class="marker-pin" id="markerPinElement">
                    <i class="fa-solid fa-motorcycle" id="markerIconElement"></i>
                </div>
            `,
            iconSize: [42, 42],
            iconAnchor: [21, 21]
        });

        this.vehicleMarker = L.marker(latLng, { icon: customIcon }).addTo(this.map);
    }

    updatePosition(lat, lng, heading = 0, speed = 0, isAlarmActive = false) {
        // Validasi koordinat real: Abaikan jika bernilai 0.00000 (GPS sedang mencari sinyal satelit)
        if (!lat || !lng || isNaN(lat) || isNaN(lng) || (Math.abs(lat) < 0.0001 && Math.abs(lng) < 0.0001)) {
            const addressEl = document.getElementById('addressDisplay');
            if (addressEl) {
                addressEl.innerHTML = '<span style="color: var(--accent-orange);"><i class="fa-solid fa-satellite-dish fa-spin"></i> Menunggu sinyal satelit GPS real... (Bawa alat ke luar ruangan / dekat jendela)</span>';
            }
            return;
        }

        const newLatLng = [lat, lng];
        const isFirstFix = (this.lastLat === null || this.lastLng === null);
        this.lastLat = lat;
        this.lastLng = lng;

        // Update Posisi Marker
        if (this.vehicleMarker) {
            this.vehicleMarker.setLatLng(newLatLng);
            
            // Perbarui Tooltip / Popup Info Real-Time pada Marker
            const popupContent = `
                <div style="font-family: 'Inter', sans-serif; font-size: 12px; color: #fff; padding: 4px;">
                    <div style="font-weight: 700; color: #00f0ff; margin-bottom: 4px;">
                        <i class="fa-solid fa-motorcycle"></i> ${APP_CONFIG.DEFAULT_VEHICLE_ID.toUpperCase()}
                    </div>
                    <div><b>Kecepatan:</b> ${speed.toFixed(1)} km/h</div>
                    <div><b>Koordinat:</b> ${lat.toFixed(6)}, ${lng.toFixed(6)}</div>
                    <div style="margin-top: 6px;">
                        <a href="https://www.google.com/maps?q=${lat},${lng}" target="_blank" style="color: #00e676; text-decoration: underline;">
                            <i class="fa-solid fa-arrow-up-right-from-square"></i> Buka di Google Maps
                        </a>
                    </div>
                </div>
            `;
            this.vehicleMarker.bindPopup(popupContent);
        }

        // Putar Ikon Motor sesuai Heading/Arah Kompas
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

        // Tambahkan ke jejak lintasan (Polyline)
        this.pathCoordinates.push(newLatLng);
        if (this.pathCoordinates.length > APP_CONFIG.MAP.MAX_POLYLINE_POINTS) {
            this.pathCoordinates.shift();
        }
        this.trailPolyline.setLatLngs(this.pathCoordinates);

        // Auto Center jika diaktifkan atau saat pertama kali mendapat sinyal GPS real
        if (this.autoCenter || isFirstFix) {
            this.map.panTo(newLatLng, { animate: true, duration: 0.5 });
            if (isFirstFix) {
                this.map.setZoom(17);
            }
        }

        // Lakukan reverse geocoding untuk menampilkan nama jalan real
        this._reverseGeocode(lat, lng);
    }

    setGeofence(centerLat, centerLng, radiusMeters) {
        if (this.geofenceCircle) {
            this.geofenceCircle.setLatLng([centerLat, centerLng]);
            this.geofenceCircle.setRadius(radiusMeters);
        }
    }

    toggleGeofence(visible) {
        if (visible) {
            this.geofenceCircle.addTo(this.map);
        } else {
            this.map.removeLayer(this.geofenceCircle);
        }
    }

    centerOnVehicle() {
        if (this.lastLat && this.lastLng) {
            this.autoCenter = true;
            this.map.flyTo([this.lastLat, this.lastLng], 17, { duration: 1 });
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
        // Debounce / limit reverse geocode requests
        const now = Date.now();
        if (this._lastGeocodeTime && now - this._lastGeocodeTime < 10000) return;
        this._lastGeocodeTime = now;

        const addressEl = document.getElementById('addressDisplay');
        if (!addressEl) return;

        fetch(`https://nominatim.openstreetmap.org/reverse?format=json&lat=${lat}&lon=${lng}&zoom=18&addressdetails=1`)
            .then(res => res.json())
            .then(data => {
                if (data && data.display_name) {
                    addressEl.textContent = data.display_name;
                    addressEl.title = data.display_name;
                }
            })
            .catch(() => {
                addressEl.textContent = `Koordinat: ${lat.toFixed(6)}, ${lng.toFixed(6)}`;
            });
    }
}
