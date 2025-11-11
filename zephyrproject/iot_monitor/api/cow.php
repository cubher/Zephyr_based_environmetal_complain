<?php

// api/cow.php
require_once __DIR__ . '/../db.php';

// GET/POST parameters:
//  - api_key (required)
//  - value (required) base64 string of image
//  - source (optional) device id or description
//  - ts (optional) timestamp

$api_key = $_REQUEST['api_key'] ?? null;
$value = $_REQUEST['value'] ?? null;
$source = $_REQUEST['source'] ?? null;
$ts = $_REQUEST['ts'] ?? null;

header('Content-Type: application/json');

// Validate API key
if (!check_api_key($api_key)) {
    http_response_code(401);
    echo json_encode(['status' => 'error', 'message' => 'Invalid API key']);
    exit;
}

// Validate base64 image data
if (empty($value)) {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Missing value parameter (base64 image)']);
    exit;
}

// Timestamp handling
$recorded_at = date('Y-m-d H:i:s');
if ($ts) {
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
    // ✅ Decode image only for saving to file
    $imgData = base64_decode($value);
    if ($imgData === false) {
        throw new Exception("Invalid base64 image data");
    }

    // ✅ Save to /uploads/cow_images/
    $uploadDir = __DIR__ . '/../uploads/cow_images/';
    if (!is_dir($uploadDir)) {
        mkdir($uploadDir, 0777, true);
    }

    $filename = 'cow_' . date('Ymd_His') . '.jpg';
    $filePath = $uploadDir . $filename;
    file_put_contents($filePath, $imgData);

    // ✅ Store in DB (keep base64 in value column)
    $stmt = $pdo->prepare("
        INSERT INTO cow_detections (value, image_path, recorded_at, source)
        VALUES (:value, :image_path, :recorded_at, :source)
    ");
    $stmt->execute([
        ':value' => $value,  // store original base64 string
        ':image_path' => 'uploads/cow_images/' . $filename,
        ':recorded_at' => $recorded_at,
        ':source' => $source
    ]);

    echo json_encode([
        'status' => 'ok',
        'message' => 'Cow image stored successfully',
        'file' => $filename,
        'inserted_id' => $pdo->lastInsertId()
    ]);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
}
