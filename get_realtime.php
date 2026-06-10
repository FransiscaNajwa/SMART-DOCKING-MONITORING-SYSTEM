<?php
header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Pragma: no-cache");
header("Expires: 0");

$realtimePath = __DIR__ . DIRECTORY_SEPARATOR . "data_jarak.txt";
$defaultPayload = [
    "jarak" => 0,
    "sisi" => "Nilam Utara",
    "status" => "MENUNGGU DATA...",
    "kecepatan" => 0,
    "timestamp" => null,
    "timestamp_ms" => null
];

if (!file_exists($realtimePath)) {
    echo json_encode($defaultPayload);
    exit;
}

$rawRealtime = file_get_contents($realtimePath);
$realtime = json_decode((string) $rawRealtime, true);

if (!is_array($realtime)) {
    echo json_encode($defaultPayload);
    exit;
}

echo json_encode(array_merge($defaultPayload, $realtime));
?>
