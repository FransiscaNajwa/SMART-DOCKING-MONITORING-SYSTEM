<?php
header("Content-Type: application/json; charset=UTF-8");
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Pragma: no-cache");
header("Expires: 0");

$configPath = __DIR__ . DIRECTORY_SEPARATOR . "dermaga_sisi.txt";
$defaultSisi = "Nilam Utara";
$allowedSisi = ["Nilam Utara", "Nilam Selatan"];

if (!file_exists($configPath)) {
    file_put_contents($configPath, $defaultSisi);
}

$sisi = trim((string) file_get_contents($configPath));
if ($sisi === "" || !in_array($sisi, $allowedSisi, true)) {
    $sisi = $defaultSisi;
    file_put_contents($configPath, $sisi);
}

echo json_encode([
    "sisi" => $sisi
]);
?>
