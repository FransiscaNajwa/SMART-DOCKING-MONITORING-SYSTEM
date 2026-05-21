<?php
date_default_timezone_set('Asia/Jakarta');

// Konfigurasi Database Laragon
$host = "localhost";
$username = "root";
$password = ""; // Default Laragon password adalah kosong
$dbname = "db_dermaga";

// 1. Membuat koneksi awal ke MySQL Server
$conn = new mysqli($host, $username, $password);

// Periksa koneksi
if ($conn->connect_error) {
    die("Koneksi ke database gagal: " . $conn->connect_error);
}

// 2. Membuat database jika belum ada
$sqlCreateDB = "CREATE DATABASE IF NOT EXISTS `$dbname` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
if (!$conn->query($sqlCreateDB)) {
    die("Gagal membuat database: " . $conn->error);
}

// 3. Memilih database db_dermaga
$conn->select_db($dbname);
$conn->set_charset("utf8mb4");
$conn->query("SET time_zone = '+07:00'");

// 4. Membuat tabel tb_log_kapal jika belum ada
$sqlCreateTable = "CREATE TABLE IF NOT EXISTS `tb_log_kapal` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `timestamp` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `dermaga_sisi` VARCHAR(50) NOT NULL DEFAULT 'Nilam Barat',
    `jarak` INT NOT NULL,
    `status` VARCHAR(30) NOT NULL,
    `kecepatan` FLOAT NOT NULL DEFAULT 0.0,
    INDEX (`timestamp`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

if (!$conn->query($sqlCreateTable)) {
    die("Gagal membuat tabel tb_log_kapal: " . $conn->error);
}
?>
