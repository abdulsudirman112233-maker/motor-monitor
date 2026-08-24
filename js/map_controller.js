// =============================================================================
// ENGINE PETA LEAFLET.JS - PURE REAL-TIME GPS TRACKING & MULTI-LAYER ENGINE
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

        // Siapkan Tile Layers Google Maps Resmi & Dark Mode
        this.tileLayers.googleRoadmap = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_GOOGLE_ROADMAP, {
            maxZoom: 20,
            subdomains: ['0', '1', '2', '3']
        });

        this.tileLayers.googleSatellite = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_GOOGLE_SATELLITE, {
            maxZoom: 20,
            subdomains: ['0', '1', '2', '3']
        });

        this.tileLayers.googleTraffic = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_GOOGLE_TRAFFIC, {
            maxZoom: 20,
            subdomains: ['0', '1', '2', '3']
        });

        this.tileLayers.dark = L.tileLayer(APP_CONFIG.MAP.TILE_LAYER_DARK, {
            maxZoom: 19,
            subdomains: 'abcd'
        });

        // Set layer default (Google Maps Roadmap)
        this.currentLayer = 'googleRoadmap';
        this.tileLayers.googleRoadmap.addTo(this.map);

        // Buat custom marker kendaraan di titik GPS real awal
        this._createVehicleMarker(APP_CONFIG.MAP.DEFAULT_CENTER);

        // Inisialisasi garis jejak rute (Polyline)
        this.trailPolyline = L.polyline([APP_CONFIG.MAP.DEFAULT_CENTER], {
            color: '#00f0ff',
            weight: 4,
            opacity: 0.85,
            smoothFactor: 1,
            lineJoin: 'round',
            dashArray: '2, 6'
        }).addTo(this.map);

        // Inisialisasi Geofence Circle (Pagar Virtual 20 Meter)
        this.geofenceCircle = L.circle(APP_CONFIG.MAP.DEFAULT_CENTER, {
            color: '#7928ca',
            fillColor: '#7928ca',
            fillOpacity: 0.18,
            weight: 2,
            dashArray: '6, 6',
            radius: APP_CONFIG.MAP.DEFAULT_GEOFENCE_RADIUS || 20
        }).addTo(this.map);
        this.geofenceCircle.bindTooltip('<i class="fa-solid fa-shield-halved"></i> <b>Pagar Virtual Geofence: 20 Meter</b>', { direction: 'top' });

        // Event saat user menggeser peta manual (matikan auto-center sementara)
        this.map.on('dragstart', () => {
            this.autoCenter = false;
            const btnCenter = document.getElementById('btnAutoCenter');
            if (btnCenter) btnCenter.classList.remove('active');
        });

        // Auto-fix rendering Leaflet pada Vercel / Cloud Container
        setTimeout(() => {
            if (this.map) this.map.invalidateSize();
        }, 300);

        setTimeout(() => {
            if (this.map) this.map.invalidateSize();
        }, 1000);

        window.addEventListener('resize', () => {
            if (this.map) this.map.invalidateSize();
        });

        // Tampilkan alamat titik awal
        this._reverseGeocode(APP_CONFIG.MAP.DEFAULT_CENTER[0], APP_CONFIG.MAP.DEFAULT_CENTER[1]);

        console.log('[MAP] Leaflet Map Initialized at Real GPS Coordinates.');
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
        this.lastLat = latLng[0];
        this.lastLng = latLng[1];
    }

    updatePosition(lat, lng, heading = 0, speed = 0, isAlarmActive = false) {
        const numLat = parseFloat(lat);
        const numLng = parseFloat(lng);

        // Validasi koordinat real: Abaikan jika bernilai 0.00000 (GPS sedang mencari satelit)
        if (!numLat || !numLng || isNaN(numLat) || isNaN(numLng) || (Math.abs(numLat) < 0.001 && Math.abs(numLng) < 0.001)) {
            const addressEl = document.getElementById('addressDisplay');
            if (addressEl) {
                addressEl.innerHTML = '<span style="color: var(--accent-orange);"><i class="fa-solid fa-satellite-dish fa-spin"></i> Menunggu sinyal satelit GPS real...</span>';
            }
            return;
        }

        const newLatLng = [numLat, numLng];
        this.lastLat = numLat;
        this.lastLng = numLng;

        // 1. Buat / Geser Marker
        if (!this.vehicleMarker) {
            this._createVehicleMarker(newLatLng);
        } else {
            this.vehicleMarker.setLatLng(newLatLng);
        }

        // 2. Perbarui Popup
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

        // 3. Putar Ikon Motor sesuai Heading
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

        // 4. Tambahkan ke lintasan
        this.pathCoordinates.push(newLatLng);
        if (this.pathCoordinates.length > APP_CONFIG.MAP.MAX_POLYLINE_POINTS) {
            this.pathCoordinates.shift();
        }
        if (this.trailPolyline) {
            this.trailPolyline.setLatLngs(this.pathCoordinates);
        }

        // 5. Auto Center
        if (this.autoCenter) {
            this.map.panTo(newLatLng, { animate: true, duration: 0.5 });
        }

        // 6. Reverse Geocoding
        this._reverseGeocode(numLat, numLng);
    }

    setGeofence(centerLat, centerLng, radiusMeters) {
        if (this.geofenceCircle) {
            this.geofenceCircle.setLatLng([centerLat, centerLng]);
            this.geofenceCircle.setRadius(radiusMeters);
            const label = radiusMeters >= 1000 ? `${(radiusMeters / 1000).toFixed(1)} KM` : `${radiusMeters} Meter`;
            this.geofenceCircle.bindTooltip(`<i class="fa-solid fa-shield-halved"></i> <b>Pagar Virtual: ${label}</b>`, { direction: 'top' });
        }
    }

    setGeofenceRadius(radiusMeters) {
        if (this.geofenceCircle) {
            this.geofenceCircle.setRadius(radiusMeters);
            const label = radiusMeters >= 1000 ? `${(radiusMeters / 1000).toFixed(1)} KM` : `${radiusMeters} Meter`;
            this.geofenceCircle.bindTooltip(`<i class="fa-solid fa-shield-halved"></i> <b>Pagar Virtual: ${label}</b>`, { direction: 'top' });
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
        if (this.trailPolyline) {
            this.trailPolyline.setLatLngs([]);
        }
    }

    _reverseGeocode(lat, lng) {
        const now = Date.now();
        if (this._lastGeocodeTime && now - this._lastGeocodeTime < 6000) return;
        this._lastGeocodeTime = now;

        const addressEl = document.getElementById('addressDisplay');
        if (!addressEl) return;

        const apiKey = (APP_CONFIG.MAP && APP_CONFIG.MAP.GOOGLE_MAPS_API_KEY) ? APP_CONFIG.MAP.GOOGLE_MAPS_API_KEY : '';

        // 1. Coba Google Maps Geocoding API Resmi (Sangat Akurat untuk Wilayah Indonesia)
        if (apiKey && apiKey.startsWith('AIzaSy')) {
            const googleUrl = `https://maps.googleapis.com/maps/api/geocode/json?latlng=${lat},${lng}&key=${apiKey}&language=id`;
            fetch(googleUrl)
                .then(res => res.json())
                .then(data => {
                    if (data.results && data.results.length > 0) {
                        const formattedAddress = data.results[0].formatted_address;
                        addressEl.innerHTML = `<i class="fa-solid fa-location-dot" style="color: var(--accent-green);"></i> <span>${formattedAddress}</span>`;
                        addressEl.title = formattedAddress;
                        return;
                    }
                    this._fallbackNominatimGeocode(lat, lng, addressEl);
                })
                .catch(() => {
                    this._fallbackNominatimGeocode(lat, lng, addressEl);
                });
        } else {
            this._fallbackNominatimGeocode(lat, lng, addressEl);
        }
    }

    _fallbackNominatimGeocode(lat, lng, addressEl) {
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
