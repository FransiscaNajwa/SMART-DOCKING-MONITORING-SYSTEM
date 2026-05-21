# SMART DOCKING MONITORING SYSTEM

Sistem ini digunakan untuk memonitor kondisi kapal di area dermaga menggunakan ESP32, sensor ultrasonik, OLED, buzzer, LED indikator, dan dashboard web berbasis PHP.

## Fitur Utama

- Monitoring jarak kapal secara real-time
- Status kapal:
  - `KAPAL SANDAR`
  - `MULAI SANDAR`
  - `MULAI LEPAS...`
  - `LEPAS`
- Tampilan OLED untuk status `MASUK` dan `KELUAR`
- Sinkronisasi data sisi dermaga dari web ke ESP32
- Dashboard web lokal menggunakan Laragon / Apache
- Simulasi dengan Wokwi dan PlatformIO

## Logika Status

Logika utama pada sistem:

- `0 - 150 cm` => `KAPAL SANDAR`
- `151 - 200 cm`:
  - jika jarak makin kecil => `MULAI SANDAR`
  - jika jarak makin besar => `MULAI LEPAS...`
- `> 200 cm` => `LEPAS`

## Teknologi yang Digunakan

- ESP32
- Arduino Framework
- PlatformIO
- Wokwi
- PHP
- HTML, CSS, JavaScript
- Laragon / Apache

## Struktur File Penting

- [kapal.ino](./kapal.ino)  
  Sketch utama ESP32 untuk sensor, OLED, LED, buzzer, dan HTTP request ke server.

- [platformio.ini](./platformio.ini)  
  Konfigurasi build PlatformIO.

- [wokwi.toml](./wokwi.toml)  
  Konfigurasi simulasi Wokwi.

- [index.html](./index.html)  
  Halaman dashboard monitoring.

- [script.js](./script.js)  
  Logic frontend dashboard.

- [style.css](./style.css)  
  Styling dashboard.

- [update.php](./update.php)  
  Endpoint untuk menerima data dari ESP32.

- [get_sisi.php](./get_sisi.php)  
  Endpoint untuk mengambil sisi dermaga aktif.

- [set_sisi.php](./set_sisi.php)  
  Endpoint untuk mengubah sisi dermaga.

- [stream.php](./stream.php)  
  Endpoint streaming / pembaruan data.

- [data_jarak.txt](./data_jarak.txt)  
  Penyimpanan data jarak terbaru saat runtime.

- [last_sample.json](./last_sample.json)  
  Penyimpanan sampel data terbaru saat runtime.

## Cara Menjalankan Web Lokal

Pastikan Laragon atau Apache sudah aktif.

Buka di browser:

```txt
http://localhost/dermaga/
```

Atau dari jaringan lokal kantor:

```txt
http://IP_SERVER_ANDA/dermaga/
```

Contoh yang dipakai saat ini di sketch:

```txt
http://192.168.0.105/dermaga/
```

## Cara Menjalankan ESP32 / PlatformIO

1. Buka folder project ini di VS Code
2. Pastikan extension PlatformIO sudah aktif
3. Build project
4. Upload ke board ESP32
5. Buka Serial Monitor untuk melihat status koneksi dan URL server

## Cara Menjalankan Simulasi Wokwi

1. Buka folder project ini sebagai root workspace di VS Code
2. Pastikan file `wokwi.toml` ada di root project
3. Build firmware melalui PlatformIO
4. Jalankan Wokwi Simulator

File build yang dipakai Wokwi:

- `.pio/build/esp32dev/firmware.bin`
- `.pio/build/esp32dev/firmware.elf`

## Konfigurasi Jaringan

Di [kapal.ino](./kapal.ino), sesuaikan bagian ini jika IP server lokal berubah:

```cpp
const char *serverHost = "192.168.0.105";
const uint16_t serverPort = 80;
```

Pastikan:

- ESP32 dan server berada di jaringan WiFi yang sama
- Apache / Laragon aktif
- Firewall Windows mengizinkan akses ke Apache bila dibutuhkan

## Catatan

- File runtime seperti `data_jarak.txt` dan `last_sample.json` tidak disertakan dalam workflow source control utama.
- Jika IP lokal berubah, cukup ubah konfigurasi `serverHost` pada sketch.
- Untuk penggunaan kantor, mode WiFi lokal lebih stabil dibanding tunnel publik.

## Author

Fransisca Najwa

