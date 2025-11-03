<?php
// api/flame.php
require_once __DIR__ . '/../db.php';

// GET parameters:
//  - api_key (required)
//  - status (required) 1 or 0
//  - source (optional)
//  - ts (optional)

$api_key = $_GET['api_key'] ?? null;
$status = $_GET['status'] ?? null;
$source = $_GET['source'] ?? null;
$ts = $_GET['ts'] ?? null;

if (!check_api_key($api_key)) {
    http_response_code(401);
    echo json_encode(['status' => 'error', 'message' => 'Invalid API key']);
    exit;
}

if ($status === null || !in_array($status, ['0','1','00','01'], true)) {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Missing or invalid status parameter (use 1 or 0)']);
    exit;
}

$recorded_at = date('Y-m-d H:i:s');
if ($ts) {
    if (ctype_digit($ts)) {
        $recorded_at = date('Y-m-d H:i:s', (int)$ts);
    } else {
        $d = date_create($ts);
        if ($d) $recorded_at = $d->format('Y-m-d H:i:s');
    }
}

try {
    $stmt = $pdo->prepare("INSERT INTO flame_events (status, recorded_at, source) VALUES (:status, :recorded_at, :source)");
    $stmt->execute([
        ':status' => (int)$status,
        ':recorded_at' => $recorded_at,
        ':source' => $source
    ]);
    echo json_encode(['status' => 'ok', 'inserted_id' => $pdo->lastInsertId()]);
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
}
