<?php

// api/air.php
require_once __DIR__ . '/../db.php';

// GET parameters:
//  - api_key (required)
//  - value (required) numeric
//  - source (optional) e.g., device id
//  - ts (optional) ISO datetime or unix timestamp (if not given server time used)

$api_key = $_GET['api_key'] ?? null;
$value = $_GET['value'] ?? null;
$source = $_GET['source'] ?? null;
$ts = $_GET['ts'] ?? null;

if (!check_api_key($api_key)) {
    http_response_code(401);
    echo json_encode(['status' => 'error', 'message' => 'Invalid API key']);
    exit;
}

if ($value === null || !is_numeric($value)) {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Missing or invalid value parameter']);
    exit;
}

// parse timestamp if provided
$recorded_at = date('Y-m-d H:i:s');
if ($ts) {
    // try numeric (unix) or date string
    if (ctype_digit($ts)) {
        $recorded_at = date('Y-m-d H:i:s', (int)$ts);
    } else {
        $d = date_create($ts);
        if ($d) {
            $recorded_at = $d->format('Y-m-d H:i:s');
        }
    }
}

try {
    $stmt = $pdo->prepare("INSERT INTO air_quality (value, recorded_at, source) VALUES (:value, :recorded_at, :source)");
    $stmt->execute([
        ':value' => (float)$value,
        ':recorded_at' => $recorded_at,
        ':source' => $source
    ]);
    echo json_encode(['status' => 'ok', 'inserted_id' => $pdo->lastInsertId()]);
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
}
