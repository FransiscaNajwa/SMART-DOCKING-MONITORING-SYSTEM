// ==========================================
// MONITORING DERMAGA REAL-TIME JAVASCRIPT
// ==========================================

let liveEventSource = null;
let fallbackPollingId = null;
let lastRealtimeTimestamp = null;
let logsPollingId = null;
const THRESHOLD_SANDAR_MAX = 150;
const THRESHOLD_LEPAS_MIN = 200;

document.addEventListener("DOMContentLoaded", function () {
    startDigitalClock();
    startRealtimeUpdates();

    const saveButton = document.getElementById("dashboard-sisi-save");
    if (saveButton) {
        saveButton.addEventListener("click", saveDermagaLocation);
    }
});

// 1. FUNGSI JAM DIGITAL UTAMA
function startDigitalClock() {
    setInterval(() => {
        const now = new Date();
        const timeString = now.toLocaleTimeString('id-ID', { hour12: false });
        const clockElement = document.getElementById("clock");
        if (clockElement) clockElement.innerText = timeString;
    }, 1000);
}

function startRealtimeUpdates() {
    fetchLatestDockData();
    connectRealtimeStream();
    fetchLatestLogs();
    if (logsPollingId === null) {
        logsPollingId = setInterval(fetchLatestLogs, 3000);
    }
}

function connectRealtimeStream() {
    if (typeof EventSource === "undefined") {
        ensureFallbackPolling();
        return;
    }

    if (liveEventSource) {
        liveEventSource.close();
    }

    liveEventSource = new EventSource("stream.php?t=" + Date.now());

    liveEventSource.onmessage = function (event) {
        try {
            const data = JSON.parse(event.data);
            applyRealtimeData(data);
            stopFallbackPolling();
        } catch (error) {
            console.error("Gagal membaca stream realtime:", error);
        }
    };

    liveEventSource.onerror = function () {
        ensureFallbackPolling();
    };
}

function ensureFallbackPolling() {
    if (fallbackPollingId !== null) return;
    fallbackPollingId = setInterval(fetchLatestDockData, 1000);
}

function stopFallbackPolling() {
    if (fallbackPollingId === null) return;
    clearInterval(fallbackPollingId);
    fallbackPollingId = null;
}

// 2. FUNGSI UTAMA FETCH AJAX DATA REAL-TIME FROM LARAGON
function fetchLatestDockData() {
    fetch('get_realtime.php?t=' + Date.now())
        .then(response => response.json())
        .then(data => {
            if (!data) return;
            applyRealtimeData(data);
        })
        .catch(error => {
            console.error("Koneksi database Laragon terputus:", error);
            setWifiDisconnected();
        });
}

function applyRealtimeData(data) {
    const jarak = parseInt(data.jarak, 10) || 0;
    const status = data.status || "MENUNGGU DATA...";
    const sisi = data.sisi || "Nilam Utara";
    const timestamp = data.timestamp_ms || data.timestamp || null;

    if (timestamp && lastRealtimeTimestamp === timestamp) {
        setWifiConnected();
        return;
    }

    lastRealtimeTimestamp = timestamp;
    document.getElementById("web-jarak").innerText = jarak;
    document.getElementById("web-sisi").innerText = sisi.replace(/_/g, ' ');

    const selectEl = document.getElementById("dashboard-sisi-input");
    if (selectEl) {
        selectEl.value = sisi;
    }

    setWifiConnected();
    updateDashboardVisuals(jarak, status);
}

function setWifiConnected() {
    const wifiIcon = document.getElementById("wifi-icon");
    if (wifiIcon) wifiIcon.className = "fa-solid fa-wifi icon-connected";
}

function setWifiDisconnected() {
    const wifiIcon = document.getElementById("wifi-icon");
    if (wifiIcon) wifiIcon.className = "fa-solid fa-wifi icon-disconnected";
}

function classifyDockState(jarak) {
    if (jarak >= 0 && jarak <= THRESHOLD_SANDAR_MAX) return "SANDAR";
    if (jarak > THRESHOLD_LEPAS_MIN) return "LEPAS";
    return "TRANSISI";
}

// 3. FUNGSI ANIMASI VISUAL & PEWARNAAN MARQUEE DASHBOARD
function updateDashboardVisuals(jarak, status) {
    const statusBox = document.getElementById("web-status");
    const shipHull = document.getElementById("ship-hull");
    const logicRows = {
        sandar: document.getElementById("logic-row-sandar"),
        transisi: document.getElementById("logic-row-transisi"),
        lepas: document.getElementById("logic-row-lepas")
    };

    // Bersihkan semua class style lama sebelum mengganti ke class baru
    statusBox.className = "status-marquee";
    shipHull.className = "ship";
    Object.values(logicRows).forEach(row => {
        if (row) row.classList.remove("row-active-sandar", "row-active-lepas", "row-active-kosong");
    });

    const dockState = classifyDockState(jarak);
    statusBox.innerText = status;

    // Kondisi Real-Time Mengikuti Posisi Jarak Kapal
    if (dockState === "SANDAR") {
        statusBox.classList.add("status-m-sandar"); // Ubah warna box status jadi hijau
        shipHull.classList.add("state-sandar");      // Geser posisi animasi kapal merapat ke dinding
        if (logicRows.sandar) logicRows.sandar.classList.add("row-active-sandar");
    } else if (dockState === "TRANSISI") {
        statusBox.classList.add("status-m-lepas");   // Ubah warna box status jadi kuning
        shipHull.classList.add("state-transisi");    // Posisi kapal berada di tengah-tengah
        if (logicRows.transisi) logicRows.transisi.classList.add("row-active-lepas");
    } else {
        statusBox.classList.add("status-m-kosong");  // Ubah warna box status jadi merah
        shipHull.classList.add("state-kosong");      // Kapal menjauh/keluar dari layout dermaga
        if (logicRows.lepas) logicRows.lepas.classList.add("row-active-kosong");
    }
}

function fetchLatestLogs() {
    fetch("get_logs.php?t=" + Date.now())
        .then(response => response.json())
        .then(logs => {
            if (!Array.isArray(logs)) return;
            renderLogs(logs);
        })
        .catch(error => {
            console.error("Gagal mengambil data log:", error);
        });
}

function renderLogs(logs) {
    const logContainer = document.getElementById("log-container");
    if (!logContainer) return;

    if (logs.length === 0) {
        logContainer.innerHTML = '<tr><td colspan="4" style="text-align:center; padding:10px;">[SISTEM] Belum ada data log.</td></tr>';
        return;
    }

    logContainer.innerHTML = logs.map(log => {
        const sisi = (log.sisi || "Nilam Utara").replace(/_/g, " ");
        return `
            <tr>
                <td>${log.timestamp}</td>
                <td>${sisi}</td>
                <td>${log.jarak} cm</td>
                <td>${log.status}</td>
            </tr>
        `;
    }).join("");
}

// 4. FUNGSI MENGIRIM INPUT LOKASI DERMAGA KE ESP32 VIA BACKEND
function saveDermagaLocation() {
    const selectEl = document.getElementById("dashboard-sisi-input");
    const feedbackEl = document.getElementById("dashboard-sisi-feedback");
    const selectedValue = selectEl.value;

    feedbackEl.innerText = "Menyimpan...";

    // Kirim data ke backend set_sisi.php menggunakan POST/GET
    fetch(`set_sisi.php?sisi=${encodeURIComponent(selectedValue)}`)
        .then(response => response.text())
        .then(res => {
            feedbackEl.innerText = "Lokasi dermaga aktif: " + selectedValue;
        })
        .catch(err => {
            feedbackEl.innerText = "Gagal memperbarui lokasi.";
            console.error(err);
        });
}
