const jarakText = document.getElementById('web-jarak');
const statusBox = document.getElementById('web-status');
const wifiIcon = document.getElementById('wifi-icon');
const shipHull = document.getElementById('ship-hull');
const logContainer = document.getElementById('log-container');
const webSpeed = document.getElementById('web-speed');
const webSisi = document.getElementById('web-sisi');
const sisiInput = document.getElementById('dashboard-sisi-input');
const sisiSaveButton = document.getElementById('dashboard-sisi-save');
const sisiFeedback = document.getElementById('dashboard-sisi-feedback');
const sisiGetUrl = 'get_sisi.php';
const sisiSetUrl = 'set_sisi.php';

const rowSandar = document.querySelector('.row-sandar');
const rowLepas = document.querySelector('.row-lepas');
const rowKosong = document.querySelector('.row-kosong');
let realtimeStream = null;

let lastJarak = -1;
let lastStatus = "";
let lastRealtimeTimestamp = "";
let lastRealtimeMs = 0;
let currentSisi = "Nilam Utara";

function getAllowedSisi() {
    if (!sisiInput) return ["Nilam Utara", "Nilam Selatan"];
    const options = Array.from(sisiInput.options || []).map(option => option.value.trim()).filter(Boolean);
    return options.length ? options : ["Nilam Utara", "Nilam Selatan"];
}

function updateShipPosition(jarak) {
    if (!shipHull) return;

    const minTop = 26;
    const maxTop = 210;
    const maxVisualDistance = 300;
    const safeJarak = Math.max(0, Number.parseInt(jarak, 10) || 0);
    const clampedJarak = Math.min(safeJarak, maxVisualDistance);
    const ratio = clampedJarak / maxVisualDistance;
    const top = minTop + ((maxTop - minTop) * ratio);

    shipHull.style.setProperty('--ship-top', `${top.toFixed(0)}px`);
}

function processRealtimeData(data) {
    const jarakAsli = parseInt(data.jarak, 10);
    if (isNaN(jarakAsli)) return;

    wifiIcon.className = "fa-solid fa-wifi icon-connected";
    jarakText.innerText = jarakAsli;
    updateShipPosition(jarakAsli);

    if (typeof data.sisi === "string" && data.sisi.trim() !== "") {
        applySisiToUi(data.sisi.trim());
    }

    const incomingRealtimeMs = Number.parseInt(data.timestamp_ms, 10) || 0;
    if (incomingRealtimeMs > lastRealtimeMs) {
        lastRealtimeMs = incomingRealtimeMs;
        lastRealtimeTimestamp = data.timestamp || lastRealtimeTimestamp;
        ambilLogsDatabase();
    } else if (data.timestamp && data.timestamp !== lastRealtimeTimestamp) {
        lastRealtimeTimestamp = data.timestamp;
        ambilLogsDatabase();
    }

    if (data.status !== lastStatus) {
        lastStatus = data.status;
        ambilLogsDatabase();
    }

    clearTableHighlights();
    if (jarakAsli > 0 && jarakAsli <= 100) {
        statusBox.innerText = "KAPAL SANDAR";
        statusBox.className = "status-marquee status-m-sandar";
        shipHull.className = "ship state-sandar";
        rowSandar.className = "row-sandar row-active-sandar";
    }
    else if (jarakAsli > 100 && jarakAsli <= 200) {
        statusBox.innerText = "MULAI LEPAS...";
        statusBox.className = "status-marquee status-m-lepas";
        shipHull.className = "ship state-lepas";
        rowLepas.className = "row-lepas row-active-lepas";
    }
    else {
        statusBox.innerText = "KOSONG / PERGI";
        statusBox.className = "status-marquee status-m-kosong";
        shipHull.className = "ship state-kosong";
        rowKosong.className = "row-kosong row-active-kosong";
    }

    lastJarak = jarakAsli;
}

// Fallback polling jika browser/server tidak mendukung EventSource
function ambilDataLokal() {
    fetch('data_jarak.txt?t=' + new Date().getTime(), { cache: 'no-store' })
        .then(response => {
            if (!response.ok) throw new Error("HTTP Error");
            return response.json();
        })
        .then(processRealtimeData)
        .catch(() => {
            wifiIcon.className = "fa-solid fa-wifi icon-disconnected";
            statusBox.innerText = "SERVER OFFLINE";
            statusBox.className = "status-marquee status-m-kosong";
        });
}

function startRealtimeStream() {
    if (!window.EventSource) {
        return false;
    }

    if (realtimeStream) {
        realtimeStream.close();
    }

    realtimeStream = new EventSource('stream.php');
    realtimeStream.onmessage = event => {
        try {
            const data = JSON.parse(event.data);
            processRealtimeData(data);
        } catch (error) {
            console.error('Gagal memproses stream real-time:', error);
        }
    };

    realtimeStream.onerror = () => {
        wifiIcon.className = "fa-solid fa-wifi icon-disconnected";
        realtimeStream.close();
        realtimeStream = null;
        setTimeout(() => {
            startRealtimeStream();
            ambilDataLokal();
        }, 1500);
    };

    return true;
}

// Fungsi mengambil data log riwayat dari database secara berkala
function ambilLogsDatabase() {
    fetch('get_logs.php?t=' + new Date().getTime())
        .then(response => {
            if (!response.ok) throw new Error("HTTP Error");
            return response.json();
        })
        .then(logs => {
            logContainer.innerHTML = ''; // Kosongkan list log lama
            
            if (logs.length === 0) {
                logContainer.innerHTML = '<div class="log-item" style="color: #64748b;">[SISTEM] Belum ada rekaman log kapal di database.</div>';
                webSpeed.innerText = "0.00 m/s";
                return;
            }

            const latestLog = logs[0];
            const latestSpeed = Math.abs(Number.parseFloat(latestLog.kecepatan || 0));
            webSpeed.innerText = latestSpeed.toFixed(2) + " m/s";

            // Tampilkan baris demi baris log
            logs.forEach(log => {
                const logBaru = document.createElement('div');
                logBaru.className = 'log-item';
                
                // Menentukan warna teks log berdasarkan status kejadian
                let color = "#38bdf8"; // Biru cerah (default)
                let label = "INFO";
                
                if (log.status === "KAPAL SANDAR") {
                    color = "#10b981"; // Emerald Green
                    label = "DOCK";
                } else if (log.status === "MULAI LEPAS...") {
                    color = "#f59e0b"; // Warning Amber
                    label = "MOVE";
                } else {
                    color = "#64748b"; // Muted Slate Grey
                    label = "IDLE";
                }
                
                logBaru.style.color = color;
                const kecepatan = Math.abs(Number.parseFloat(log.kecepatan || 0));
                
                // Format isi string log lengkap
                logBaru.innerText = `[${log.timestamp}] ${label} | Sisi: ${log.sisi} | Jarak: ${log.jarak} cm | Kecep: ${kecepatan.toFixed(2)} m/s | ${log.status}`;
                logContainer.appendChild(logBaru);
            });
        })
        .catch(err => {
            console.error("Gagal menarik log dari database:", err);
        });
}

function clearTableHighlights() {
    rowSandar.className = "row-sandar";
    rowLepas.className = "row-lepas";
    rowKosong.className = "row-kosong";
}

function applySisiToUi(sisi) {
    const allowedSisi = getAllowedSisi();
    currentSisi = allowedSisi.includes(sisi) ? sisi : sisi;
    if (webSisi) webSisi.innerText = currentSisi;
    if (sisiInput) {
        const hasOption = Array.from(sisiInput.options || []).some(option => option.value === currentSisi);
        if (hasOption) sisiInput.value = currentSisi;
    }
    if (sisiFeedback) sisiFeedback.innerText = `Lokasi dermaga aktif: ${currentSisi}`;
}

function loadConfiguredSisi() {
    fetch(sisiGetUrl + '?t=' + new Date().getTime())
        .then(response => {
            if (!response.ok) throw new Error("HTTP Error");
            return response.json();
        })
        .then(data => {
            applySisiToUi(data.sisi || "Nilam Utara");
        })
        .catch(() => {
            applySisiToUi("Nilam Utara");
        });
}

function saveConfiguredSisi() {
    if (!sisiInput) return;
    const sisi = sisiInput.value.trim();
    const allowedSisi = getAllowedSisi();
    if (!allowedSisi.includes(sisi)) {
        if (sisiFeedback) sisiFeedback.innerText = "Pilihan lokasi dermaga tidak valid.";
        return;
    }

    if (sisiFeedback) sisiFeedback.innerText = "Menyimpan lokasi dermaga...";
    if (sisiSaveButton) sisiSaveButton.disabled = true;

    fetch(sisiSetUrl + '?sisi=' + encodeURIComponent(sisi) + '&t=' + new Date().getTime(), {
        method: 'GET',
        cache: 'no-store'
    })
        .then(response => {
            if (!response.ok) throw new Error("HTTP Error");
            return response.json();
        })
        .then(data => {
            if (!data.success) throw new Error(data.message || "Gagal menyimpan");
            return fetch(sisiGetUrl + '?t=' + new Date().getTime(), { cache: 'no-store' })
                .then(checkResponse => {
                    if (!checkResponse.ok) throw new Error("HTTP Error");
                    return checkResponse.json();
                })
                .then(savedData => {
                    const sisiTersimpan = savedData.sisi || data.sisi || sisi;
                    applySisiToUi(sisiTersimpan);

                    const logBaru = document.createElement('div');
                    logBaru.className = 'log-item';
                    logBaru.style.color = '#38bdf8';
                    logBaru.innerText = `[${new Date().toLocaleTimeString()}] SISTEM | Lokasi dermaga diatur ke ${sisiTersimpan}`;
                    logContainer.prepend(logBaru);
                    ambilLogsDatabase();
                });
        })
        .catch(() => {
            if (sisiFeedback) sisiFeedback.innerText = "Gagal menyimpan lokasi dermaga.";
        })
        .finally(() => {
            if (sisiSaveButton) sisiSaveButton.disabled = false;
        });
}

// Gunakan stream real-time bila tersedia, fallback ke polling tiap 500ms
if (!startRealtimeStream()) {
    setInterval(ambilDataLokal, 500);
}

ambilDataLokal();

// Polling log dari database MySQL setiap 1000ms agar tampilan UI lebih sinkron dengan DB
setInterval(ambilLogsDatabase, 1000);

// Panggil pertama kali saat startup
ambilLogsDatabase();
loadConfiguredSisi();

// Jam Digital Header
setInterval(() => {
    document.getElementById('clock').innerText = new Date().toLocaleTimeString();
}, 1000);

if (sisiSaveButton) {
    sisiSaveButton.addEventListener('click', saveConfiguredSisi);
    sisiSaveButton.onclick = saveConfiguredSisi;
}
