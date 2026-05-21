<?php
date_default_timezone_set('Asia/Jakarta');

header('Content-Type: text/event-stream; charset=UTF-8');
header('Cache-Control: no-cache, no-store, must-revalidate');
header('Pragma: no-cache');
header('Expires: 0');
header('X-Accel-Buffering: no');

$dataPath = __DIR__ . DIRECTORY_SEPARATOR . 'data_jarak.txt';
$lastHash = '';

function kirimEvent(array $payload): void
{
    echo 'data: ' . json_encode($payload) . "\n\n";
    @ob_flush();
    flush();
}

for ($i = 0; $i < 30; $i++) {
    clearstatcache(false, $dataPath);

    if (file_exists($dataPath)) {
        $raw = file_get_contents($dataPath);
        if ($raw !== false && $raw !== '') {
            $hash = md5($raw);
            if ($hash !== $lastHash) {
                $decoded = json_decode($raw, true);
                if (is_array($decoded)) {
                    kirimEvent($decoded);
                    $lastHash = $hash;
                }
            }
        }
    }

    if (connection_aborted()) {
        break;
    }

    usleep(500000);
}

echo ": stream-end\n\n";
@ob_flush();
flush();
?>
