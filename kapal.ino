#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>

// ---------- Pin definitions ----------
const int pinTrig = 5;
const int pinEcho = 18;
const int pinLedSandar = 26; // Green LED (near)
const int pinLedKosong = 27; // Red LED (far)
const int pinBuzzer = 25;
const int pinSda = 21;
const int pinScl = 22;

const int ledSandarChannel = 1;
const int ledKosongChannel = 2;
const int buzzerChannel = 0;

// ---------- OLED ----------
const uint8_t screenWidth = 128;
const uint8_t screenHeight = 64;
const int oledReset = -1;
const uint8_t oledAddress = 0x3C;
Adafruit_SSD1306 display(screenWidth, screenHeight, &Wire, oledReset);

// ---------- WiFi / Server ----------
const char *ssid = "Wifi_TPKN";
const char *password = "PelindoTPKN";
const char *serverHost = "192.168.0.105";
const uint16_t serverPort = 80;
const char *serverPath = "/dermaga/update.php";
const char *sisiPath = "/dermaga/get_sisi.php";

String serverUrl;
String sisiUrl;
String dermagaSisi = "Nilam Utara";
String lastKondisi = "";
unsigned long lastMsg = 0;
unsigned long lastSisiSync = 0;

enum ShipState { SHIP_ARRIVE, SHIP_DEPART };
ShipState currentState = SHIP_DEPART;

// Forward declarations for IntelliSense / C++ parser
void setBuzzer(bool active, unsigned int frequency = 700);
void setLedSandar(uint8_t brightness);
void setLedKosong(uint8_t brightness);
void triggerLedSekali(const String &kondisiBaru);
void sinkronkanSisiDariWeb(bool force = false);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void attachPwmChannel(int pin, int frequency, int resolution, int channel) {
  ledcAttach(pin, frequency, resolution);
}

void writePwmValue(int pin, int channel, uint8_t value) {
  ledcWrite(pin, value);
}

void writeToneValue(int pin, int channel, unsigned int frequency) {
  ledcWriteTone(pin, frequency);
}
#else
void attachPwmChannel(int pin, int frequency, int resolution, int channel) {
  ledcSetup(channel, frequency, resolution);
  ledcAttachPin(pin, channel);
}

void writePwmValue(int pin, int channel, uint8_t value) {
  ledcWrite(channel, value);
}

void writeToneValue(int pin, int channel, unsigned int frequency) {
  ledcWriteTone(channel, frequency);
}
#endif

String parseJsonSisi(const String &payload) {
  const String key = "\"sisi\"";
  int keyIndex = payload.indexOf(key);
  if (keyIndex < 0)
    return "";

  int colonIndex = payload.indexOf(':', keyIndex + key.length());
  if (colonIndex < 0)
    return "";

  int firstQuote = payload.indexOf('"', colonIndex + 1);
  if (firstQuote < 0)
    return "";

  int secondQuote = payload.indexOf('"', firstQuote + 1);
  if (secondQuote < 0)
    return "";

  String sisi = payload.substring(firstQuote + 1, secondQuote);
  sisi.trim();
  return sisi;
}

String formatSisiForQuery(const String &sisi) {
  String encoded = sisi;
  encoded.replace(" ", "_");
  return encoded;
}

void renderOled(const String &line1, const String &line2 = "",
                const String &line3 = "", const String &line4 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2.length() > 0)
    display.println(line2);
  if (line3.length() > 0)
    display.println(line3);
  if (line4.length() > 0)
    display.println(line4);
  display.display();
}

long readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long dur = pulseIn(echoPin, HIGH);
  long cm = dur * 0.034 / 2;
  if (cm <= 0)
    cm = 300;
  return cm;
}

void renderStatusScreen(long jarak, const String &status) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Monitor Dermaga");
  display.println(dermagaSisi);

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(jarak);
  display.print(" cm");

  display.setTextSize(1);
  display.setCursor(0, 48);
  display.println(status);
  display.display();
}

void renderStatusScreen2(long jarak, const String &masukStatus,
                         const String &keluarStatus) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Monitor Dermaga");
  display.println(dermagaSisi);

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(jarak);
  display.print(" cm");

  display.setTextSize(1);
  display.setCursor(0, 44);
  display.print("MASUK: ");
  display.println(masukStatus);

  display.setCursor(0, 54);
  display.print("KELUAR: ");
  display.println(keluarStatus);
  display.display();
}
// ==================== REVISI FUNGSI LOGIKA STATUS ====================

String getStatusAktif(long jarak, long prevJarak) {
  if (jarak >= 0 && jarak <= 150) return "KAPAL SANDAR";
  if (jarak > 200)                return "LEPAS";

  // Logika mendeteksi arah pergerakan di zona abu-abu (151 - 200 cm)
  if (prevJarak != -1) { 
    if (jarak > prevJarak) {
      return "MULAI LEPAS..."; // Jarak makin besar = kapal menjauh
    } else if (jarak < prevJarak) {
      return "MULAI SANDAR";   // Jarak makin kecil = kapal mendekat
    }
  }
  return "MULAI SANDAR"; // Default jika baru booting atau jarak stabil
}

String getMasukStatus(long jarak, long prevJarak) {
  String statusAktif = getStatusAktif(jarak, prevJarak);
  if (statusAktif == "KAPAL SANDAR" || statusAktif == "MULAI SANDAR") {
    return statusAktif;
  }
  return "LEPAS"; // Jika kapal menjauh/pergi, status Masuk dianggap clear/lepas
}

String getKeluarStatus(long jarak, long prevJarak) {
  String statusAktif = getStatusAktif(jarak, prevJarak);
  if (statusAktif == "LEPAS" || statusAktif == "MULAI LEPAS...") {
    return statusAktif;
  }
  return "KAPAL SANDAR"; // Jika kapal mendekat, status Keluar diset stand-by
}

// ==================== REVISI VOID LOOP (NON-BLOCKING) ====================

void loop() {
  sinkronkanSisiDariWeb();

  // 1. Pembacaan Sensor Ultrasonik secara berkala tanpa interupsi
  static long prevJarak = -1;
  long jarak = readDistance(pinTrig, pinEcho);
  
  String statusAktif = getStatusAktif(jarak, prevJarak);
  String masuk = getMasukStatus(jarak, prevJarak);
  String keluar = getKeluarStatus(jarak, prevJarak);

  // 2. Logika Output LED Utama (Solid State)
  if (jarak <= 150) {
    setLedSandar(255);
    setLedKosong(0);
    currentState = SHIP_ARRIVE;
  } else if (jarak <= 200) {
    // Di zona transisi (151-200), matikan kedua LED solid, biarkan logika transisi bekerja
    setLedSandar(0);
    setLedKosong(0);
  } else {
    setLedSandar(0);
    setLedKosong(255);
    currentState = SHIP_DEPART;
  }

  // 3. Update Tampilan Layar OLED 
  renderStatusScreen2(jarak, masuk, keluar);

  // 4. Pengiriman Data ke Web Server Laragon (Tiap 500ms)
  unsigned long now = millis();
  if (now - lastMsg > 500) {
    lastMsg = now;
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String requestPath = serverUrl + "?jarak=" + String(jarak) +
                           "&sisi=" + formatSisiForQuery(dermagaSisi) +
                           "&status=" + statusAktif +
                           "&statusMasuk=" + masuk +
                           "&statusKeluar=" + keluar;
      http.setTimeout(2000); // Dipercepat ke 2 detik agar tidak lagging
      http.begin(requestPath.c_str());
      http.GET();
      http.end();
    }
  }

  // 5. Logika Buzzer & Trigger LED Transisi menggunakan Millis (Bebas Delay)
  static unsigned long lastBuzzerToggle = 0;
  static bool buzzerState = false;

  if (jarak > 0 && jarak <= 150) {
    triggerLedSekali("KAPAL SANDAR");
    setBuzzer(false);
  } 
  else if (jarak > 150 && jarak <= 200) {
    lastKondisi = statusAktif;
    
    // Membuat buzzer berkedip tiap 250ms tanpa menghentikan pembacaan sensor (Non-Blocking)
    if (now - lastBuzzerToggle > 250) {
      lastBuzzerToggle = now;
      buzzerState = !buzzerState;
      setBuzzer(buzzerState, 700);
    }
  } 
  else {
    triggerLedSekali("LEPAS");
    setBuzzer(false);
  }

  // PENTING: Update data prevJarak HANYA di akhir loop setelah semua logika selesai dihitung
  prevJarak = jarak; 
  delay(50); // Delay kecil agar pembacaan sensor stabil dan tidak membebani prosesor ESP32
}
