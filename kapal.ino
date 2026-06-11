#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Wire.h>

// ---------- Konfigurasi Server Lokal Laragon ----------
// SIMULASI WOKWI + NGROK:
//   1. Jalankan Laragon (Apache + MySQL)
//   2. Buka terminal di folder ini dan jalankan: .\run_ngrok.bat
//   3. Ganti officeServerHost dengan domain ngrok (contoh: xxxx-xx-xxx.ngrok.io)
//   4. Ubah officeServerPort ke 443 (HTTPS ngrok)
//   5. Jalankan simulasi Wokwi
// 
// SIMULASI WOKWI + LOCALHOST (tanpa ngrok):
//   - Jalankan Laragon dengan port 80
//   - Gunakan officeServerHost: 127.0.0.1 atau host.wokwi.internal
//   - Ubah officeServerPort ke 80
//
// HARDWARE REAL (ESP32 fisik):
//   - Ubah officeServerHost ke IP server/router di jaringan lokal
//   - Contoh: 192.168.1.100 atau 10.0.0.50
//   - Gunakan officeServerPort sesuai konfigurasi server Anda
//
// CATATAN: Auto-detect protocol -> port 443 = HTTPS, port 80 = HTTP
const char *officeServerHost = "127.0.0.1";      // Localhost untuk test lokal
const uint16_t officeServerPort = 80;             // Port Laragon (80=HTTP, 443=HTTPS ngrok)
const char *serverPath = "/dermaga/dermaga/update.php";
const char *sisiPath = "/dermaga/dermaga/get_sisi.php";

// ---------- Pin definitions ----------
const int pinTrig = 5;
const int pinEcho = 18;
const int pinLedSandar = 27; // Green LED (sandar)
const int pinLedKosong = 26; // Red LED (lepas)
const int pinBuzzer = 25;
const int pinSda = 21;
const int pinScl = 22;

const int ledSandarChannel = 1;
const int ledKosongChannel = 2;
const int buzzerChannel = 0;

// ---------- LCD ----------
const uint8_t lcdAddress = 0x27;
const uint8_t lcdColumns = 20;
const uint8_t lcdRows = 4;
LiquidCrystal_I2C display(lcdAddress, lcdColumns, lcdRows);

String serverUrl;
String sisiUrl;
String dermagaSisi = "";
String lastKondisi = "";
unsigned long lastMsg = 0;
unsigned long lastSisiSync = 0;
const long transitionNoiseThreshold = 2;

enum ShipState { SHIP_ARRIVE, SHIP_DEPART };
ShipState currentState = SHIP_DEPART;

// Tambahkan variabel pendukung untuk monitoring perubahan real-time
long lastSentDistance = -999; 
String lastSentStatus = "";

// ---------- FORWARD DECLARATIONS ----------
void setBuzzer(bool active, unsigned int frequency = 700);
void setLedSandar(uint8_t brightness);
void setLedKosong(uint8_t brightness);
void fadeLed(int pin, int channel, uint8_t fromBrightness, uint8_t toBrightness, int stepDelayMs = 6);
void warmSisi(String sisiBaru);
void sinkronkanSisiDariWeb(bool force = false);
void kirimDataKeWeb(long jarakAktif, String statusAktif);
String encodeQueryValue(const String &value);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void attachPwmChannel(int pin, int frequency, int resolution, int channel) { ledcAttach(pin, frequency, resolution); }
void writePwmValue(int pin, int channel, uint8_t value) { ledcWrite(pin, value); }
void writeToneValue(int pin, int channel, unsigned int frequency) { ledcWriteTone(pin, frequency); }
#else
void attachPwmChannel(int pin, int frequency, int resolution, int channel) { ledcSetup(channel, frequency, resolution); ledcAttachPin(pin, channel); }
void writePwmValue(int pin, int channel, uint8_t value) { ledcWrite(channel, value); }
void writeToneValue(int pin, int channel, unsigned int frequency) { ledcWriteTone(channel, frequency); }
#endif

// ---------- HELPER FUNCTIONS ----------
String parseJsonSisi(const String &payload) {
  const String key = "\"sisi\"";
  int keyIndex = payload.indexOf(key);
  if (keyIndex < 0) return "";
  int colonIndex = payload.indexOf(':', keyIndex + key.length());
  if (colonIndex < 0) return "";
  int firstQuote = payload.indexOf('"', colonIndex + 1);
  if (firstQuote < 0) return "";
  int secondQuote = payload.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return "";
  String sisi = payload.substring(firstQuote + 1, secondQuote);
  sisi.trim();
  return sisi;
}

String formatSisiForQuery(const String &sisi) {
  String encoded = sisi;
  encoded.replace(" ", "_");
  return encoded;
}

String encodeQueryValue(const String &value) {
  String encoded = "";
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    bool isAlphaNum = (c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9');
    if (isAlphaNum || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

String fitLcdLine(const String &text) {
  String line = text;
  if (line.length() > lcdColumns) line = line.substring(0, lcdColumns);
  while (line.length() < lcdColumns) line += ' ';
  return line;
}

String lastLcdLines[4] = {"", "", "", ""};

void printLcdLine(uint8_t row, const String &text) {
  String nextLine = fitLcdLine(text);
  if (lastLcdLines[row] == nextLine) return;
  display.setCursor(0, row);
  display.print(nextLine);
  lastLcdLines[row] = nextLine;
}

void resetLcdCache() {
  for (int i = 0; i < 4; i++) { lastLcdLines[i] = ""; }
}

void renderOled(const String &line1, const String &line2 = "", const String &line3 = "", const String &line4 = "") {
  printLcdLine(0, line1);
  printLcdLine(1, line2);
  printLcdLine(2, line3);
  printLcdLine(3, line4);
}

long readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long dur = pulseIn(echoPin, HIGH, 30000);
  long cm = dur * 0.034 / 2;
  if (cm <= 0) cm = 300;
  return cm;
}

void renderStatusScreen(long jarak, const String &status) {
  printLcdLine(0, "Monitor Dermaga");
  printLcdLine(1, "Sisi: " + (dermagaSisi == "" ? "Memuat..." : dermagaSisi));
  printLcdLine(2, "Jarak: " + String(jarak) + " cm");
  printLcdLine(3, "Status: " + status);
}

String getStatusAktif(long jarak, long prevJarak) {
  if (jarak >= 0 && jarak <= 150) return "KAPAL SANDAR";
  if (jarak > 200) return "LEPAS";
  if (prevJarak != -1) {
    long delta = jarak - prevJarak;
    if (delta >= transitionNoiseThreshold) return "MULAI LEPAS...";
    if (delta <= -transitionNoiseThreshold) return "MULAI SANDAR";
  }
  if (lastKondisi == "MULAI SANDAR" || lastKondisi == "MULAI LEPAS...") return lastKondisi;
  return "TRANSISI";
}

void warmSisi(String sisiBaru) {
  sisiBaru.replace("_", " ");
  dermagaSisi = sisiBaru;
}

void setBuzzer(bool active, unsigned int frequency) {
  if (active) writeToneValue(pinBuzzer, buzzerChannel, frequency);
  else writeToneValue(pinBuzzer, buzzerChannel, 0);
}

void setLedSandar(uint8_t brightness) { writePwmValue(pinLedSandar, ledSandarChannel, brightness); }
void setLedKosong(uint8_t brightness) { writePwmValue(pinLedKosong, ledKosongChannel, brightness); }

void fadeLed(int pin, int channel, uint8_t fromBrightness, uint8_t toBrightness, int stepDelayMs) {
  if (fromBrightness == toBrightness) {
    writePwmValue(pin, channel, fromBrightness);
    return;
  }
  int step = fromBrightness < toBrightness ? 5 : -5;
  for (int level = fromBrightness; (step > 0) ? (level <= toBrightness) : (level >= toBrightness); level += step) {
    writePwmValue(pin, channel, level);
    delay(stepDelayMs);
  }
  writePwmValue(pin, channel, toBrightness);
}

void indikatorMati() {
  setLedSandar(0);
  setLedKosong(0);
  setBuzzer(false);
}

void selfTestIndikator() {
  fadeLed(pinLedSandar, ledSandarChannel, 0, 255, 4);
  fadeLed(pinLedSandar, ledSandarChannel, 255, 0, 4);
  fadeLed(pinLedKosong, ledKosongChannel, 0, 255, 4);
  fadeLed(pinLedKosong, ledKosongChannel, 255, 0, 4);
  setBuzzer(true, 900);
  delay(200);
  setBuzzer(false);
  delay(100);
}

void sinkronkanSisiDariWeb(bool force) {
  if (WiFi.status() != WL_CONNECTED) return;
  unsigned long now = millis();
  if (!force && now - lastSisiSync < 1000) return;
  lastSisiSync = now;

  HTTPClient http;
  String requestUrl = sisiUrl + "?t=" + String(now);
  http.setTimeout(1500); // Dipercepat responnya agar tidak lag
  http.begin(requestUrl.c_str());
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String payload = http.getString();
    String sisiBaru = parseJsonSisi(payload);
    if (sisiBaru.length() > 0) warmSisi(sisiBaru);
  }
  http.end();
}

// Fungsi utama pengiriman data real-time bypass tanpa nunggu delay lama
void kirimDataKeWeb(long jarakAktif, String statusAktif) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String requestPath = serverUrl + "?jarak=" + String(jarakAktif) +
                         "&sisi=" + encodeQueryValue(dermagaSisi) +
                         "&status=" + encodeQueryValue(statusAktif);
    http.setTimeout(500); 
    http.begin(requestPath.c_str());
    int httpResponseCode = http.GET();
    http.end();
    
    if (httpResponseCode > 0) {
      lastSentDistance = jarakAktif;
      lastSentStatus = statusAktif;
    }
  }
}

// ---------- MAIN LOOP ----------
void loop() {
  sinkronkanSisiDariWeb();

  static long prevJarak = -1;
  long jarak = readDistance(pinTrig, pinEcho);
  if (prevJarak == -1) prevJarak = jarak;

  String statusAktif = getStatusAktif(jarak, prevJarak);
  lastKondisi = statusAktif;

  // LOGIKA REAL-TIME: Jika jarak bergeser atau status berubah, langsung paksa kirim data saat ini juga!
  if (jarak != lastSentDistance || statusAktif != lastSentStatus) {
    kirimDataKeWeb(jarak, statusAktif);
  }

  static String lastLedState = "";
  if (jarak <= 150) {
    if (lastLedState != "SANDAR") {
      setLedKosong(0);
      setLedSandar(0);
      fadeLed(pinLedSandar, ledSandarChannel, 0, 255, 4);
      lastLedState = "SANDAR";
    }
    currentState = SHIP_ARRIVE;
  } else if (jarak > 200) {
    if (lastLedState != "LEPAS") {
      setLedSandar(0);
      setLedKosong(0);
      fadeLed(pinLedKosong, ledKosongChannel, 0, 255, 4);
      lastLedState = "LEPAS";
    }
    currentState = SHIP_DEPART;
  } else {
    if (lastLedState != "TRANSISI") {
      setLedSandar(0);
      setLedKosong(0);
      lastLedState = "TRANSISI";
    }
  }

  renderStatusScreen(jarak, statusAktif);

  // Pengiriman rutin berkala setiap 500ms jika data cenderung diam (statis)
  unsigned long now = millis();
  if (now - lastMsg > 500) {
    lastMsg = now;
    kirimDataKeWeb(jarak, statusAktif);
  }

  static unsigned long lastBuzzerToggle = 0;
  static bool buzzerState = false;

  if (jarak > 0 && jarak <= 150) {
    setLedSandar(255);
    setLedKosong(0);
    setBuzzer(false);
  } else if (jarak > 150 && jarak <= 200) {
    setLedSandar(0);
    setLedKosong(0);
    if (now - lastBuzzerToggle > 250) {
      lastBuzzerToggle = now;
      buzzerState = !buzzerState;
      setBuzzer(buzzerState, 700);
    }
  } else {
    setLedSandar(0);
    setLedKosong(255);
    setBuzzer(false);
  }

  prevJarak = jarak;
  delay(30); // Dioptimalkan sedikit lebih cepat respon loopnya
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  pinMode(pinTrig, OUTPUT);
  pinMode(pinEcho, INPUT);
  pinMode(pinLedSandar, OUTPUT);
  pinMode(pinLedKosong, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);

  attachPwmChannel(pinLedSandar, 5000, 8, ledSandarChannel);
  attachPwmChannel(pinLedKosong, 5000, 8, ledKosongChannel);
  attachPwmChannel(pinBuzzer, 2000, 8, buzzerChannel);
  indikatorMati();

  Wire.begin(pinSda, pinScl);
  display.init();
  display.backlight();
  display.clear();
  resetLcdCache();

  renderOled("Booting LCD...", "Inisialisasi OK");
  selfTestIndikator();

  renderOled("Connect to Wokwi", "Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("#");
  }

  renderOled("WiFi Connected!", WiFi.localIP().toString());
  delay(1500);

  // Auto-detect protocol berdasarkan port (80=HTTP, 443=HTTPS)
  String protocol = (officeServerPort == 443) ? "https://" : "http://";
  serverUrl = protocol + String(officeServerHost) + ":" + String(officeServerPort) + String(serverPath);
  sisiUrl = protocol + String(officeServerHost) + ":" + String(officeServerPort) + String(sisiPath);

  sinkronkanSisiDariWeb(true);

  Serial.println();
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Server URL: ");
  Serial.println(serverUrl);

  renderStatusScreen(0, "Sistem Siap");
}
