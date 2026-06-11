# Wokwi Simulation Setup Guide

## Cara Menjalankan Simulasi Wokwi dengan Laragon

### Prerequisites
- Laragon sudah terinstall
- ngrok sudah terinstall atau tersedia di folder proyek
- Wokwi Online atau VS Code dengan extension Wokwi

### Step 1: Jalankan Laragon
1. Buka Laragon
2. Klik tombol **Start All** untuk menjalankan Apache dan MySQL
3. Pastikan status menunjukkan **Running**

### Step 2: Setup ngrok Tunnel
1. Buka PowerShell/Terminal di folder proyek ini
2. Jalankan:
   ```bash
   .\run_ngrok.bat
   ```
3. ngrok akan menampilkan URL seperti: `https://xxxx-xxxx-xxxx.ngrok.io`
4. **Catat URL ini**

### Step 3: Update Firmware Configuration
Di file `kapal.ino`, ubah baris:
```cpp
const char *officeServerHost = "127.0.0.1";  // Ubah ke ngrok URL
const uint16_t officeServerPort = 80;         // Tetap 80
const char *serverPath = "/dermaga/dermaga/update.php";
```

Menjadi:
```cpp
const char *officeServerHost = "xxxx-xxxx-xxxx.ngrok.io";  // Ganti dengan URL ngrok Anda
const uint16_t officeServerPort = 443;                      // HTTPS port
const char *serverPath = "/dermaga/dermaga/update.php";
```

### Step 4: Compile & Run Wokwi
1. Di VS Code, buka folder proyek
2. Compile dengan PlatformIO: `pio run`
3. Buka file `diagram.json` di Wokwi
4. Jalankan simulasi

### Troubleshooting

**ESP32 tidak bisa connect WiFi:**
- Pastikan Wokwi menggunakan network emulation yang benar
- Di Wokwi dashboard, setup network dengan gateway pointing ke Laragon host

**Request timeout/connection refused:**
- Periksa ngrok tunnel masih aktif
- Verify URL ngrok di firmware match dengan terminal
- Check firewall tidak memblock ngrok

**Database error:**
- Pastikan MySQL di Laragon sudah running
- Check `db.php` bisa reach `localhost:3306`
- Lihat Laragon logs untuk error detail

## Alternative: Local Network (Advanced)

Jika ingin tanpa ngrok, setup Wokwi network forwarding:
1. Di VS Code, install extension: **Wokwi**
2. Di `diagram.json`, tambahkan:
```json
"env": {
  "WOKWI_NETWORK": "host"
}
```
3. ESP32 akan bisa akses `host.wokwi.internal` yang point ke machine lokal

Ubah firmware menjadi:
```cpp
const char *officeServerHost = "host.wokwi.internal";
const uint16_t officeServerPort = 80;
```

## Production Deployment

Untuk hardware ESP32 real di jaringan lokal:

1. Cari IP address ESP32 atau server:
   ```bash
   ipconfig
   ```

2. Update firmware:
   ```cpp
   const char *officeServerHost = "192.168.1.100";  // IP server Anda
   const uint16_t officeServerPort = 80;
   ```

3. Compile dan upload ke ESP32
