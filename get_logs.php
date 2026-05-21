<?php
// Mengizinkan halaman web dashboard membaca file ini tanpa terblokir keamanan browser (CORS)
header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Pragma: no-cache");
header("Expires: 0");

// Hubungkan ke database
require_once "db.php";

// Query 20 data log teratas/terbaru
$query = "SELECT * FROM `tb_log_kapal` ORDER BY `id` DESC LIMIT 20";
$result = $conn->query($query);

$logs = [];
if ($result && $result->num_rows > 0) {
    while ($row = $result->fetch_assoc()) {
        $logs[] = [
            "id" => intval($row['id']),
            "timestamp" => $row['timestamp'],
            "sisi" => $row['dermaga_sisi'],
            "jarak" => intval($row['jarak']),
            "status" => $row['status'],
            "kecepatan" => floatval($row['kecepatan'])
        ];
    }
}

echo json_encode($logs);
?>
