<?php
// Hubungkan ke database
require_once "db.php";

// Set header untuk download file CSV/Excel
header('Content-Type: text/csv; charset=utf-8');
header('Content-Disposition: attachment; filename=log_aktivitas_kapal_' . date('Y-m-d_H-i-s') . '.csv');

// Hindari caching
header('Cache-Control: no-cache, no-store, must-revalidate');
header('Pragma: no-cache');
header('Expires: 0');

// Buka output stream untuk menulis CSV secara dinamis
$output = fopen('php://output', 'w');

// Tulis BOM (Byte Order Mark) UTF-8 agar karakter bahasa dan pemisah dibaca dengan benar oleh Excel
fprintf($output, chr(0xEF).chr(0xBB).chr(0xBF));

// Tambahkan baris header judul kolom Excel
fputcsv($output, array(
    'No', 
    'Waktu Kejadian (Timestamp)', 
    'Sisi Dermaga', 
    'Jarak Sensor (cm)', 
    'Status Dermaga', 
    'Kecepatan Pendekatan (m/s)'
));

// Query semua log kapal untuk direkap di Excel
$query = "SELECT * FROM `tb_log_kapal` ORDER BY `id` DESC";
$result = $conn->query($query);

$no = 1;
if ($result && $result->num_rows > 0) {
    while ($row = $result->fetch_assoc()) {
        fputcsv($output, array(
            $no++,
            $row['timestamp'],
            $row['dermaga_sisi'],
            $row['jarak'] . ' cm',
            $row['status'],
            round($row['kecepatan'], 2) . ' m/s'
        ));
    }
}

// Tutup stream
fclose($output);
exit();
?>
