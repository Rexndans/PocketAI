/*
  POCKET AI v2 - ESP32-S3
  5 tombol + OLED 0.96" SPI (6 pin) -> Gemini -> log ke Telegram
  Remote IR bersifat opsional (matikan/nyalakan dengan USE_IR).

  Library (Library Manager):
   - Adafruit SSD1306
   - Adafruit GFX Library
   - ArduinoJson (v7.x)
   - IRremote (v4.x)  -> hanya kalau USE_IR = 1

  Wiring OLED (SPI):
   GND -> GND        VCC -> 3V3
   SCL -> GPIO 12    SDA -> GPIO 11
   RES -> GPIO 13    DC  -> GPIO 10

  Tombol (satu kaki ke GPIO, kaki lain ke GND):
   MUNDUR (<) -> GPIO 4     MAJU (>)  -> GPIO 5
   OK         -> GPIO 7     HAPUS     -> GPIO 15
   KIRIM      -> GPIO 16

  IR receiver: OUT -> GPIO 6, VCC -> 3V3, GND -> GND

  Cara pakai:
   < >    : pilih karakter (tahan = jalan otomatis)
   OK     : masukkan karakter ke teks
   HAPUS  : hapus 1 huruf (tahan 1 detik = hapus semua)
   KIRIM  : kirim ke Gemini
   Layar jawaban: < > ganti halaman, OK atau HAPUS = kembali menulis
   Layar mati sendiri setelah 30 detik, tombol apa pun menyalakannya lagi.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>

// ================== ISI DATA KAMU ==================
const char* WIFI_SSID  = "NAMA_WIFI";
const char* WIFI_PASS  = "PASSWORD_WIFI";
const char* TG_TOKEN   = "TOKEN_BOT_KAMU";
const char* CHAT_ID    = "CHAT_ID_KAMU";
const char* GEMINI_KEY = "API_KEY_GEMINI";
const char* MODEL      = "ISI_ID_MODEL_FLASH_DI_SINI";
// ===================================================

// ---------- Pin ----------
#define OLED_SCK   12
#define OLED_MOSI  11
#define OLED_DC    10
#define OLED_RST   13
#define OLED_CS    14   // tidak disambung (CS modul sudah ke GND)

#define PIN_PREV   4
#define PIN_NEXT   5
#define PIN_OK     7
#define PIN_DEL    15
#define PIN_SEND   16

// ---------- Opsi ----------
#define USE_IR            0       // 1 = aktifkan remote IR (isi kode di bawah)
#define IR_PIN            6
#define LAYAR_REDUP       true    // true = kecerahan diturunkan (hemat baterai)
#define TIMEOUT_LAYAR_MS  30000UL // layar mati setelah 30 detik tanpa tombol

#if USE_IR
#include <IRremote.hpp>
// Isi dengan kode tombol remote kamu (lihat di Serial Monitor saat menekan tombol).
// 0xFFFF = belum diisi.
const uint16_t IR_PREV = 0xFFFF;
const uint16_t IR_NEXT = 0xFFFF;
const uint16_t IR_OK   = 0xFFFF;
const uint16_t IR_DEL  = 0xFFFF;
const uint16_t IR_SEND = 0xFFFF;
#endif

// ---------- Layar ----------
#define SCREEN_W 128
#define SCREEN_H 64
#define COLS 21
#define ROWS_PAGE 7

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &SPI, OLED_DC, OLED_RST, OLED_CS);

// Urutan karakter
const char* CHARS = "abcdefghijklmnopqrstuvwxyz 0123456789.,?!-";
int NCHARS;
int idx = 0;

String teks = "";
const int MAX_TEKS = 200;

enum Mode { MODE_INPUT, MODE_JAWABAN };
Mode mode = MODE_INPUT;

std::vector<String> lines;
int halaman = 0;

bool layarMati = false;
unsigned long lastActivity = 0;

// ---------- Tombol ----------
struct Tombol {
  int pin;
  bool lastRaw = HIGH;
  bool stable = HIGH;
  unsigned long lastChange = 0;
  unsigned long pressedAt = 0;
  unsigned long lastRepeat = 0;
  bool longDone = false;
};
Tombol bPrev{PIN_PREV}, bNext{PIN_NEXT}, bOk{PIN_OK}, bDel{PIN_DEL}, bSend{PIN_SEND};

// true sekali saat tombol baru ditekan
bool tekanBaru(Tombol &b) {
  bool raw = digitalRead(b.pin);
  unsigned long now = millis();
  if (raw != b.lastRaw) { b.lastRaw = raw; b.lastChange = now; }
  if (now - b.lastChange > 30 && raw != b.stable) {
    b.stable = raw;
    if (b.stable == LOW) {
      b.pressedAt = now;
      b.lastRepeat = now;
      b.longDone = false;
      return true;
    }
  }
  return false;
}

// true berulang saat ditahan (setelah 500 ms, tiap 130 ms)
bool tahanRepeat(Tombol &b) {
  if (b.stable == LOW) {
    unsigned long now = millis();
    if (now - b.pressedAt > 500 && now - b.lastRepeat > 130) {
      b.lastRepeat = now;
      return true;
    }
  }
  return false;
}

// true sekali saat ditahan lebih dari ms
bool tahanLama(Tombol &b, unsigned long ms) {
  if (b.stable == LOW && !b.longDone && millis() - b.pressedAt > ms) {
    b.longDone = true;
    return true;
  }
  return false;
}

// ---------- Layar hidup/mati ----------
void layarNyala() { display.ssd1306_command(SSD1306_DISPLAYON);  layarMati = false; }
void layarOff()   { display.ssd1306_command(SSD1306_DISPLAYOFF); layarMati = true; }

// ---------- Tampilan ----------
void pesan(const String& a, const String& b = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(a);
  if (b.length()) display.println(b);
  display.display();
}

char labelChar(int i) {
  char c = CHARS[((i % NCHARS) + NCHARS) % NCHARS];
  return c == ' ' ? '_' : c;
}

void gambarInput() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  String tampil = teks + "_";
  if (tampil.length() > COLS * 3) tampil = tampil.substring(tampil.length() - COLS * 3);
  display.setCursor(0, 0);
  display.print(tampil);

  display.drawLine(0, 27, SCREEN_W - 1, 27, SSD1306_WHITE);

  // tetangga kiri dan kanan (kecil)
  display.setTextSize(1);
  display.setCursor(2, 44);   display.print("<");
  display.setCursor(120, 44); display.print(">");
  display.setTextSize(2);
  display.setCursor(24, 40);  display.print(labelChar(idx - 1));
  display.setCursor(92, 40);  display.print(labelChar(idx + 1));

  // karakter terpilih (besar)
  display.setTextSize(4);
  display.setCursor(52, 32);
  display.print(labelChar(idx));
  display.display();
}

void gambarJawaban() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int total = (lines.size() + ROWS_PAGE - 1) / ROWS_PAGE;
  if (total < 1) total = 1;
  int start = halaman * ROWS_PAGE;
  for (int i = 0; i < ROWS_PAGE; i++) {
    int n = start + i;
    if (n >= (int)lines.size()) break;
    display.setCursor(0, i * 8);
    display.print(lines[n]);
  }
  display.drawLine(0, 56, SCREEN_W - 1, 56, SSD1306_WHITE);
  display.setCursor(0, 57);
  display.print(halaman + 1); display.print("/"); display.print(total);
  display.print(" </> hal OK:tulis");
  display.display();
}

// ---------- Teks ----------
void wrapText(const String& s) {
  lines.clear();
  String line = "", word = "";
  for (size_t i = 0; i <= s.length(); i++) {
    char c = (i < s.length()) ? s[i] : ' ';
    if (c == ' ' || c == '\n') {
      if (word.length()) {
        if (line.length() + (line.length() ? 1 : 0) + word.length() <= COLS) {
          if (line.length()) line += ' ';
          line += word;
        } else {
          if (line.length()) lines.push_back(line);
          line = word;
          while ((int)line.length() > COLS) {
            lines.push_back(line.substring(0, COLS));
            line = line.substring(COLS);
          }
        }
        word = "";
      }
      if (c == '\n') { lines.push_back(line); line = ""; }
    } else {
      word += c;
    }
  }
  if (line.length()) lines.push_back(line);
}

// Buang karakter yang tidak bisa ditampilkan OLED (emoji, aksen, markdown)
String bersihkan(const String& s) {
  String out = "";
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = s[i];
    if (c == '\n' || (c >= 32 && c < 127)) {
      if (c == '*' || c == '#' || c == '`') continue;
      out += (char)c;
    }
  }
  out.trim();
  return out;
}

// ---------- WiFi ----------
bool pastikanWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(200);
  return WiFi.status() == WL_CONNECTED;
}

// ---------- Telegram ----------
void kirimTele(const String& text) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  if (!http.begin(c, String("https://api.telegram.org/bot") + TG_TOKEN + "/sendMessage")) return;
  http.addHeader("Content-Type", "application/json");
  JsonDocument d;
  d["chat_id"] = CHAT_ID;
  d["text"] = text.substring(0, 4000);
  String body; serializeJson(d, body);
  http.POST(body);
  http.end();
}

// ---------- Gemini ----------
String tanyaGemini(const String& q) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  String url = String("https://generativelanguage.googleapis.com/v1beta/models/") + MODEL + ":generateContent";
  if (!http.begin(c, url)) return "Error: gagal mulai koneksi";
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-goog-api-key", GEMINI_KEY);
  http.setTimeout(60000);

  JsonDocument req;
  req["systemInstruction"]["parts"][0]["text"] =
    "Kamu asisten di perangkat kecil berlayar OLED. Jawab singkat, padat, "
    "bahasa Indonesia, teks polos tanpa markdown dan tanpa emoji.";
  req["contents"][0]["parts"][0]["text"] = q;
  // Dibuat longgar supaya model yang 'berpikir' tidak kehabisan token sebelum menjawab
  req["generationConfig"]["maxOutputTokens"] = 1500;
  String body; serializeJson(req, body);

  int code = http.POST(body);
  String res = http.getString();
  http.end();

  if (code <= 0) return "Error: gagal konek (" + String(code) + ")";

  JsonDocument r;
  if (deserializeJson(r, res)) return "Error: gagal baca respon";
  const char* t = r["candidates"][0]["content"]["parts"][0]["text"];
  if (t) return String(t);

  const char* fr = r["candidates"][0]["finishReason"];
  if (fr) return "Error " + String(code) + ": " + String(fr);
  return "Error " + String(code) + ": " + res.substring(0, 150);
}

// ---------- Kirim pertanyaan ----------
void prosesKirim() {
  if (teks.length() == 0) return;
  String q = teks;

  pesan("Mikir...");
  String a;
  if (!pastikanWiFi()) a = "Error: WiFi putus";
  else a = tanyaGemini(q);

  String aBersih = bersihkan(a);
  if (aBersih.length() == 0) aBersih = "(jawaban kosong)";

  wrapText(aBersih);
  halaman = 0;
  mode = MODE_JAWABAN;
  gambarJawaban();

  kirimTele("🧑 " + q + "\n\n🤖 " + a);

  teks = "";
  idx = 0;
  lastActivity = millis();
}

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  NCHARS = strlen(CHARS);

  pinMode(PIN_PREV, INPUT_PULLUP);
  pinMode(PIN_NEXT, INPUT_PULLUP);
  pinMode(PIN_OK,   INPUT_PULLUP);
  pinMode(PIN_DEL,  INPUT_PULLUP);
  pinMode(PIN_SEND, INPUT_PULLUP);

  SPI.begin(OLED_SCK, -1, OLED_MOSI, OLED_CS);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0, true, false)) {
    Serial.println("OLED tidak terdeteksi! Cek wiring SCL/SDA/RES/DC.");
    while (true) delay(1000);
  }
  if (LAYAR_REDUP) display.dim(true);

#if USE_IR
  IrReceiver.begin(IR_PIN, DISABLE_LED_FEEDBACK);
#endif

  pesan("Pocket AI", "Konek WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    if (millis() - t0 > 20000) {
      pesan("WiFi gagal!", "Cek SSID/password");
      delay(3000);
      ESP.restart();
    }
  }
  pesan("WiFi OK", WiFi.localIP().toString());
  delay(1200);
  gambarInput();
  lastActivity = millis();
}

void loop() {
  bool aPrev = tekanBaru(bPrev); aPrev |= tahanRepeat(bPrev);
  bool aNext = tekanBaru(bNext); aNext |= tahanRepeat(bNext);
  bool aOk   = tekanBaru(bOk);
  bool aDel  = tekanBaru(bDel);
  bool aSend = tekanBaru(bSend);
  bool clearAll = tahanLama(bDel, 1000);

#if USE_IR
  if (IrReceiver.decode()) {
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      uint16_t c = IrReceiver.decodedIRData.command;
      Serial.print("IR command: 0x"); Serial.println(c, HEX);
      if (c == IR_PREV) aPrev = true;
      if (c == IR_NEXT) aNext = true;
      if (c == IR_OK)   aOk   = true;
      if (c == IR_DEL)  aDel  = true;
      if (c == IR_SEND) aSend = true;
    }
    IrReceiver.resume();
  }
#endif

  bool ada = aPrev || aNext || aOk || aDel || aSend || clearAll;
  unsigned long now = millis();

  if (!ada) {
    if (!layarMati && now - lastActivity > TIMEOUT_LAYAR_MS) layarOff();
    return;
  }

  lastActivity = now;
  if (layarMati) {          // tombol pertama hanya menyalakan layar
    layarNyala();
    return;
  }

  if (mode == MODE_INPUT) {
    if (aPrev) idx = (idx - 1 + NCHARS) % NCHARS;
    if (aNext) idx = (idx + 1) % NCHARS;
    if (aOk && (int)teks.length() < MAX_TEKS) teks += CHARS[idx];
    if (aDel && teks.length()) teks.remove(teks.length() - 1);
    if (clearAll) teks = "";
    if (aSend) { prosesKirim(); return; }
    gambarInput();
  } else {
    int total = (lines.size() + ROWS_PAGE - 1) / ROWS_PAGE;
    if (total < 1) total = 1;
    if (aNext) halaman = (halaman + 1) % total;
    if (aPrev) halaman = (halaman - 1 + total) % total;
    if (aOk || aDel) {
      mode = MODE_INPUT;
      gambarInput();
      return;
    }
    gambarJawaban();
  }
}
