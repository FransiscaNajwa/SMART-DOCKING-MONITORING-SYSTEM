<?php
header("Content-Type: application/json; charset=UTF-8");
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Pragma: no-cache");
header("Expires: 0");

$defaultSisi = "Nilam Utara";
$allowedSisi = ["Nilam Utara", "Nilam Selatan"];
$rawSisi = $_POST['sisi'] ?? $_GET['sisi'] ?? $defaultSisi;
$sisi = preg_replace('/[^a-zA-Z0-9_\- ]/', '', $rawSisi);
$sisi = str_replace('_', ' ', trim($sisi));

if ($sisi === "" || !in_array($sisi, $allowedSisi, true)) {
    $sisi = $defaultSisi;
}

$configPath = __DIR__ . DIRECTORY_SEPARATOR . "dermaga_sisi.txt";
$saved = file_put_contents($configPath, $sisi);

$realtimePath = __DIR__ . DIRECTORY_SEPARATOR . "data_jarak.txt";
if (file_exists($realtimePath)) {
    $rawRealtime = file_get_contents($realtimePath);
    $realtime = json_decode((string) $rawRealtime, true);

    if (is_array($realtime)) {
        $realtime["sisi"] = $sisi;
        file_put_contents($realtimePath, json_encode($realtime));
    }
}

echo json_encode([
    "success" => $saved !== false,
    "sisi" => $sisi
]);
?>
