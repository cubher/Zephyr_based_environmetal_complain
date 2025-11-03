<?php

// db.php
// Common DB connection and config for the app

// --- CONFIG: change these if needed ---
$dbHost = '127.0.0.1';
$dbName = 'iot_project';
$dbUser = 'root';
$dbPass = ''; // XAMPP default empty on Windows
$apiKey = 'K72E1D4G1GFUC4VZ'; // change to your secret key
// --------------------------------------

try {
    $dsn = "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4";
    $pdo = new PDO($dsn, $dbUser, $dbPass, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode(['error' => 'DB connection failed: '.$e->getMessage()]);
    exit;
}

// Helper: check API key (GET)
function check_api_key($provided)
{
    global $apiKey;
    return isset($provided) && $provided === $apiKey;
}

// CORS & JSON header helpers
header('Access-Control-Allow-Origin: *');
header('Content-Type: application/json; charset=utf-8');
