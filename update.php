<?php
date_default_timezone_set('Asia/Jakarta');

// 1. Mengizinkan halaman web dashboard membaca file ini tanpa terblokir keamanan browser (CORS)
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Content-Type: text/plain; charset=UTF-8");

// 2. Hubungkan ke database (db.php otomatis membuat database & tabel jika belum ada)
require_once "db.php";

function bacaSisiDashboard() {
    $configPath = __DIR__ . DIRECTORY_SEPARATOR . "dermaga_sisi.txt";
    $allowedSisi = ["Nilam Utara", "Nilam Selatan"];
    if (!file_exists($configPath)) {
        return null;
    }

    $isi = trim((string) file_get_contents($configPath));
    if ($isi === "") {
        return null;
    }

    $isi = preg_replace('/[^a-zA-Z0-9_\- ]/', '', $isi);
    return in_array($isi, $allowedSisi, true) ? $isi : null;
}

function bacaSampleTerakhir() {
    $samplePath = __DIR__ . DIRECTORY_SEPARATOR . "last_sample.json";
    if (!file_exists($samplePath)) {
        return null;
    }

    $raw = file_get_contents($samplePath);
    $data = json_decode((string) $raw, true);
    if (!is_array($data)) {
        return null;
    }

    return $data;
}

function simpanSampleTerakhir($jarak, $timestampMs) {
    $samplePath = __DIR__ . DIRECTORY_SEPARATOR . "last_sample.json";
    $payload = [
        "jarak" => (int) $jarak,
        "timestamp_ms" => (int) $timestampMs
    ];
    file_put_contents($samplePath, json_encode($payload));
}

function bacaStatusTransisiTerakhir() {
    $realtimePath = __DIR__ . DIRECTORY_SEPARATOR . "data_jarak.txt";
    if (!file_exists($realtimePath)) {
        return null;
    }

    $rawRealtime = file_get_contents($realtimePath);
    $realtime = json_decode((string) $rawRealtime, true);
    if (!is_array($realtime) || !isset($realtime["status"])) {
        return null;
    }

    $status = (string) $realtime["status"];
    if ($status === "MULAI SANDAR" || $status === "MULAI LEPAS...") {
        return $status;
    }

    return null;
}

// 3. Mengecek apakah ada kiriman data 'jarak' dari ESP32 Wokwi
if (isset($_GET['jarak'])) {
    $jarak = intval($_GET['jarak']); // Memastikan data berupa angka bulat
    
    // Sisi dermaga dikirim dinamik dari ESP32, default: 'Nilam Utara'
    $sisiDashboard = bacaSisiDashboard();
    $sisi = $sisiDashboard ?: (isset($_GET['sisi']) ? preg_replace('/[^a-zA-Z0-9_\- ]/', '', $_GET['sisi']) : 'Nilam Utara');
    $sisi = str_replace('_', ' ', $sisi); // Bersihkan format underscore jika ada

    // =========================================================================
    // PERBAIKAN UTAMA: Ambil status MURNI dari kiriman Wokwi ($_GET['status'])
    // =========================================================================
    if (isset($_GET['status']) && trim($_GET['status']) !== '') {
        $status = trim($_GET['status']); 
        // Bersihkan format jika Wokwi mengirim dengan underscore (misal: MULAI_LEPAS...)
        $status = str_replace('_', ' ', $status); 
    } else {
        $status = "";
    }

    // Inisialisasi kecepatan
    $kecepatan = 0.0;
    $waktuBaruMs = (int) round(microtime(true) * 1000);

    // Hitung kecepatan dari sample terakhir real-time
    $sampleTerakhir = bacaSampleTerakhir();
    if ($sampleTerakhir && isset($sampleTerakhir['jarak'], $sampleTerakhir['timestamp_ms'])) {
        $jarakSampleLama = intval($sampleTerakhir['jarak']);
        $waktuSampleLamaMs = intval($sampleTerakhir['timestamp_ms']);
        $selisihWaktuMs = $waktuBaruMs - $waktuSampleLamaMs;
        $selisihJarakSample = $jarakSampleLama - $jarak;

        if ($selisihWaktuMs > 0) {
            $kecepatan = abs(($selisihJarakSample / 100.0) / ($selisihWaktuMs / 1000.0));
            $kecepatan = round($kecepatan, 2);
        }
    }

    if ($status === "") {
        if ($jarak >= 0 && $jarak <= 150) {
            $status = "KAPAL SANDAR";
        } else if ($jarak > 200) {
            $status = "LEPAS";
        } else if ($sampleTerakhir && isset($sampleTerakhir['jarak'])) {
            $jarakSampleLama = intval($sampleTerakhir['jarak']);
            $deltaJarak = $jarak - $jarakSampleLama;
            if ($deltaJarak >= 2) {
                $status = "MULAI LEPAS...";
            } else if ($deltaJarak <= -2) {
                $status = "MULAI SANDAR";
            } else {
                $status = bacaStatusTransisiTerakhir() ?: "TRANSISI";
            }
        } else {
            $status = "TRANSISI";
        }
    }
    
    // Ambil record terakhir dari database untuk mendeteksi perubahan data
    $query = "SELECT * FROM `tb_log_kapal` ORDER BY `id` DESC LIMIT 1";
    $result = $conn->query($query);
    
    $shouldInsert = true; // Flag log database

    if ($result && $result->num_rows > 0) {
        $lastRow = $result->fetch_assoc();
        $jarakLama = intval($lastRow['jarak']);
        $statusLama = $lastRow['status'];
        $waktuLama = strtotime($lastRow['timestamp']);
        $waktuBaru = time();
        
        $selisihWaktu = $waktuBaru - $waktuLama; // Selisih dalam detik
        $selisihJarak = $jarakLama - $jarak; 

        // Jika jarak sama persis dan hasil kecepatan 0, tidak perlu simpan log baru
        if ($jarak === $jarakLama && abs($kecepatan) < 0.00001) {
            $shouldInsert = false;
        }

        // --- FILTER OPTIMALISASI DATABASE LOGS ---
        if ($status === $statusLama && abs($selisihJarak) < 3 && $selisihWaktu < 10) {
            $shouldInsert = false; // Hindari duplikasi log yang identik
        }
    }

    // Catat ke log database jika memenuhi kriteria
    if ($shouldInsert) {
        $stmt = $conn->prepare("INSERT INTO `tb_log_kapal` (`dermaga_sisi`, `jarak`, `status`, `kecepatan`) VALUES (?, ?, ?, ?)");
        $stmt->bind_param("sisd", $sisi, $jarak, $status, $kecepatan);
        $stmt->execute();
        $stmt->close();
    }

    simpanSampleTerakhir($jarak, $waktuBaruMs);

    // 4. Menyimpan status real-time ke data_jarak.txt dalam format JSON
    $realtimeData = [
        "jarak" => $jarak,
        "sisi" => $sisi,
        "status" => $status, // Status murni Wokwi berhasil di-passing ke teks stream!
        "kecepatan" => $kecepatan,
        "timestamp" => date("Y-m-d H:i:s"),
        "timestamp_ms" => $waktuBaruMs
    ];
    file_put_contents("data_jarak.txt", json_encode($realtimeData));

    // Respon balik ke ESP32
    echo "Sukses! Jarak: " . $jarak . "cm | Sisi: " . $sisi . " | Status: " . $status . " | Kecepatan: " . $kecepatan . " m/s";
} else {
    echo "Sistem Siaga. Menunggu kiriman data dari Wokwi...";
}
?>
