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
        this.hasLivePosition = false;
        this.gpsFixLost = false;
        this.lastAcceptedAt = 0;
        this.geofenceVisible = true;
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
        const previousLat = this.lastLat;
        const previousLng = this.lastLng;
        const displacement = this.hasLivePosition ? this._distanceMeters(previousLat, previousLng, numLat, numLng) : Infinity;
        const now = Date.now();
        const elapsedSeconds = this.lastAcceptedAt ? Math.max(0.1, (now - this.lastAcceptedAt) / 1000) : 1;
        const speedKmh = Number(speed) || 0;

        // Putus garis setelah GPS kehilangan fix. Titik pertama saat reacquire
        // menjadi awal segmen baru, bukan disambungkan ke posisi stale.
        const reacquiredFix = this.gpsFixLost;
        if (reacquiredFix) {
            this.pathCoordinates = [];
            if (this.trailPolyline) this.trailPolyline.setLatLngs([]);
            if (this.vehicleMarker) this.vehicleMarker.setLatLng(newLatLng);
            this.gpsFixLost = false;
        }

        // Tolak teleport GPS. Batas mengikuti kecepatan dan waktu sejak titik
        // terakhir, dengan toleransi minimum 20 meter untuk akurasi konsumen.
        const plausibleDistance = Math.max(20, (speedKmh / 3.6) * elapsedSeconds * 2.5 + 10);
        if (this.hasLivePosition && !reacquiredFix && displacement > plausibleDistance && elapsedSeconds < 30) {
            console.warn(`[MAP] Lompatan GPS ${displacement.toFixed(1)} m ditolak (batas ${plausibleDistance.toFixed(1)} m).`);
            return;
        }

        // GPS diam tetap bergeser beberapa meter. Jangan gerakkan marker atau
        // menambah trail jika speed nol dan perubahan masih dalam radius drift.
        if (this.hasLivePosition && Number(speed) <= 2.5 && displacement < 5) {
            return;
        }
        this.lastLat = numLat;
        this.lastLng = numLng;
        this.hasLivePosition = true;
        this.lastAcceptedAt = now;

        // 1. Buat / Geser Marker secara Smooth (Sliding Animation)
        if (!this.vehicleMarker) {
            this._createVehicleMarker(newLatLng);
        } else {
            this._animateMarkerTo(numLat, numLng, 1000);
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
        if (this.pathCoordinates.length === 0 || displacement >= 2) {
            this.pathCoordinates.push(newLatLng);
        }
        if (this.pathCoordinates.length > APP_CONFIG.MAP.MAX_POLYLINE_POINTS) {
            this.pathCoordinates.shift();
        }
        if (this.trailPolyline) {
            this.trailPolyline.setLatLngs(this.pathCoordinates);
        }

        // 5. Auto Center
        if (this.autoCenter && !this._animFrameId) {
            this.map.panTo(newLatLng, { animate: true, duration: 0.5 });
        }

        // 6. Reverse Geocoding
        this._reverseGeocode(numLat, numLng);
    }

    setGpsFixAvailable(available) {
        if (!available && this.hasLivePosition) {
            this.gpsFixLost = true;
            const addressEl = document.getElementById('addressDisplay');
            if (addressEl) {
                addressEl.innerHTML = '<span style="color: var(--accent-orange);"><i class="fa-solid fa-satellite-dish fa-spin"></i> GPS kehilangan fix, menunggu posisi baru...</span>';
            }
        }
    }

    _distanceMeters(lat1, lng1, lat2, lng2) {
        const toRad = value => value * Math.PI / 180;
        const dLat = toRad(lat2 - lat1);
        const dLng = toRad(lng2 - lng1);
        const a = Math.sin(dLat / 2) ** 2 +
            Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.sin(dLng / 2) ** 2;
        return 6371000 * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
    }

    _animateMarkerTo(targetLat, targetLng, duration = 1000) {
        if (!this.vehicleMarker) return;

        const startLatLng = this.vehicleMarker.getLatLng();
        const startLat = startLatLng.lat;
        const startLng = startLatLng.lng;

        const deltaLat = targetLat - startLat;
        const deltaLng = targetLng - startLng;

        // Jika pergeseran sangat kecil (< 0.1 meter), langsung tetapkan tanpa perulangan animasi
        if (Math.abs(deltaLat) < 0.000001 && Math.abs(deltaLng) < 0.000001) {
            this.vehicleMarker.setLatLng([targetLat, targetLng]);
            return;
        }

        if (this._animFrameId) {
            cancelAnimationFrame(this._animFrameId);
        }

        const startTime = performance.now();

        const animateStep = (currentTime) => {
            const elapsedTime = currentTime - startTime;
            const progress = Math.min(elapsedTime / duration, 1.0);

            // Interpolasi Linier Halus
            const currentLat = startLat + deltaLat * progress;
            const currentLng = startLng + deltaLng * progress;

            const currLatLng = [currentLat, currentLng];
            this.vehicleMarker.setLatLng(currLatLng);

            if (this.autoCenter) {
                this.map.panTo(currLatLng, { animate: false });
            }

            if (progress < 1.0) {
                this._animFrameId = requestAnimationFrame(animateStep);
            } else {
                this._animFrameId = null;
            }
        };

        this._animFrameId = requestAnimationFrame(animateStep);
    }

    setGeofence(centerLat, centerLng, radiusMeters) {
        const lat = Number(centerLat);
        const lng = Number(centerLng);
        const radius = Number(radiusMeters);
        if (this.geofenceCircle && Number.isFinite(lat) && Number.isFinite(lng) &&
            Math.abs(lat) <= 90 && Math.abs(lng) <= 180 &&
            Number.isFinite(radius) && radius >= 5 && radius <= 5000) {
            this.geofenceCircle.setLatLng([lat, lng]);
            this.geofenceCircle.setRadius(radius);
            const label = radius >= 1000 ? `${(radius / 1000).toFixed(1)} KM` : `${radius} Meter`;
            this.geofenceCircle.bindTooltip(`<i class="fa-solid fa-shield-halved"></i> <b>Pagar Virtual: ${label}</b>`, { direction: 'top' });
        }
    }

    setGeofenceRadius(radiusMeters) {
        const radius = Number(radiusMeters);
        if (this.geofenceCircle && Number.isFinite(radius) && radius >= 5 && radius <= 5000) {
            this.geofenceCircle.setRadius(radius);
            const label = radius >= 1000 ? `${(radius / 1000).toFixed(1)} KM` : `${radius} Meter`;
            this.geofenceCircle.bindTooltip(`<i class="fa-solid fa-shield-halved"></i> <b>Pagar Virtual: ${label}</b>`, { direction: 'top' });
        }
    }

    toggleGeofence(visible) {
        if (!this.geofenceCircle) return;
        this.geofenceVisible = Boolean(visible);
        if (this.geofenceVisible) {
            if (this.map.hasLayer(this.geofenceCircle)) return;
            this.geofenceCircle.addTo(this.map);
        } else if (this.map.hasLayer(this.geofenceCircle)) {
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
        if (this._lastGeocodeTime && now - this._lastGeocodeTime < 5000) return;
        this._lastGeocodeTime = now;

        const addressEl = document.getElementById('addressDisplay');
        if (!addressEl) return;

        const gmapsUrl = `https://www.google.com/maps?q=${lat},${lng}`;
        const apiKey = (APP_CONFIG.MAP && APP_CONFIG.MAP.GOOGLE_MAPS_API_KEY) ? APP_CONFIG.MAP.GOOGLE_MAPS_API_KEY : '';

        // Prioritaskan Google Maps Geocoding API Resmi dengan Pembersihan Alamat Presisi
        if (apiKey && apiKey.startsWith('AIzaSy')) {
            const googleUrl = `https://maps.googleapis.com/maps/api/geocode/json?latlng=${lat},${lng}&key=${apiKey}&language=id`;
            fetch(googleUrl)
                .then(res => res.json())
                .then(data => {
                    if (data.status === 'OK' && data.results && data.results.length > 0) {
                        let formattedAddress = data.results[0].formatted_address;
                        // Hapus teks nama wilayah/kelurahan yang tidak akurat jika ada
                        formattedAddress = formattedAddress
                            .replace(/, Kanakea Dalam/gi, '')
                            .replace(/Kanakea Dalam, /gi, '')
                            .replace(/Kanakea, /gi, '')
                            .replace(/, Kanakea/gi, '');

                        addressEl.innerHTML = `<i class="fa-solid fa-location-dot" style="color: var(--accent-green);"></i> <span>${formattedAddress}</span> (<a href="${gmapsUrl}" target="_blank" style="color: var(--primary); text-decoration: underline; font-weight: 600;">Lihat di GMaps</a>)`;
                        addressEl.title = formattedAddress;
                        return;
                    }
                    this._displayExactGmapsLocation(lat, lng, addressEl);
                })
                .catch(() => {
                    this._displayExactGmapsLocation(lat, lng, addressEl);
                });
        } else {
            this._displayExactGmapsLocation(lat, lng, addressEl);
        }
    }

    _displayExactGmapsLocation(lat, lng, addressEl) {
        const gmapsUrl = `https://www.google.com/maps?q=${lat},${lng}`;
        addressEl.innerHTML = `<i class="fa-solid fa-location-dot" style="color: var(--accent-green);"></i> <span>Koordinat Google Maps: <a href="${gmapsUrl}" target="_blank" style="color: var(--primary); text-decoration: underline; font-weight: 600;">${lat.toFixed(6)}, ${lng.toFixed(6)} (Buka di Google Maps)</a></span>`;
        addressEl.title = `Koordinat Presisi Google Maps: ${lat}, ${lng}`;
    }
}
