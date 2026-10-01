#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <TM1637Display.h>

extern "C" {
#include <user_interface.h>
}

/*
  Blaze Pisonet Timer — Public-Safe Continuation
  ------------------------------------------------
  Derived from a private embedded-audio edition.

  Public changes:
  - no embedded MP3/audio payloads;
  - no universal fixed AP password;
  - no personal/contact/deployment data;
  - passive-buzzer tone alerts only;
  - generic local web interface.

  Hardware defaults (NodeMCU-style labels):
  - coin input: GPIO0 / D3
  - relay 1: GPIO5 / D1
  - relay 2: GPIO4 / D2
  - passive buzzer: GPIO3 / RX
  - TM1637 CLK: GPIO12 / D6
  - TM1637 DIO: GPIO13 / D7
*/

ESP8266WebServer server(80);

static const uint32_t EEPROM_MAGIC = 0x42545032UL;
static const size_t EEPROM_SIZE = 512;

struct Config {
  uint32_t magic;
  char apName[33];
  char apPassword[65];
  uint8_t coinPin;
  uint8_t relay1Pin;
  uint8_t relay2Pin;
  uint8_t buzzerPin;
  uint8_t displayClk;
  uint8_t displayDio;
  bool coinActiveLow;
  bool relay1ActiveLow;
  bool relay2ActiveLow;
  uint16_t coinMinutes;
  uint8_t coinSeconds;
  uint16_t warningSeconds;
  uint8_t displayBrightness;
  uint32_t remainingSeconds;
  uint32_t lifetimeCoins;
};

Config cfg;
TM1637Display* displayUnit = nullptr;

bool timerRunning = false;
bool coinLatched = false;
uint32_t lastCoinEdgeMs = 0;
uint32_t lastTickMs = 0;
uint32_t sessionCoins = 0;

String htmlEscape(String s) {
  s.replace("&", "&amp;");
  s.replace("\"", "&quot;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  return s;
}

bool validApPassword(const String& p) {
  return p.length() == 0 || (p.length() >= 8 && p.length() <= 63);
}

String randomPassword(size_t len = 12) {
  static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  String out;
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    uint32_t r = os_random() ^ micros() ^ (ESP.getChipId() << (i % 7));
    out += alphabet[r % (sizeof(alphabet) - 1)];
    delay(1);
  }
  return out;
}

String defaultApName() {
  char b[24];
  snprintf(b, sizeof(b), "BlazeTimer-%06X", ESP.getChipId());
  return String(b);
}

void writeRelay(uint8_t pin, bool activeLow, bool on) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, (on ^ activeLow) ? HIGH : LOW);
}

void setRelays(bool on) {
  writeRelay(cfg.relay1Pin, cfg.relay1ActiveLow, on);
  writeRelay(cfg.relay2Pin, cfg.relay2ActiveLow, on);
}

void beep(uint16_t freq, uint16_t durationMs) {
  if (cfg.buzzerPin == 255) return;
  pinMode(cfg.buzzerPin, OUTPUT);
  tone(cfg.buzzerPin, freq, durationMs);
}

void beepCoin() { beep(1800, 70); }
void beepWarning() { beep(1200, 90); }
void beepTimeout() { beep(700, 350); }

void setupDisplay() {
  if (displayUnit) {
    delete displayUnit;
    displayUnit = nullptr;
  }
  displayUnit = new TM1637Display(cfg.displayClk, cfg.displayDio);
  if (displayUnit) {
    displayUnit->setBrightness(constrain(cfg.displayBrightness, 0, 7), true);
    displayUnit->clear();
  }
}

void showRemaining() {
  if (!displayUnit) return;
  uint32_t shown = cfg.remainingSeconds > 5999UL ? 5999UL : cfg.remainingSeconds;
  uint16_t mm = shown / 60;
  uint16_t ss = shown % 60;
  uint16_t value = mm * 100 + ss;
  displayUnit->showNumberDecEx(value, 0x40, true);
}

void saveConfig() {
  cfg.magic = EEPROM_MAGIC;
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

void loadDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = EEPROM_MAGIC;
  defaultApName().toCharArray(cfg.apName, sizeof(cfg.apName));
  randomPassword().toCharArray(cfg.apPassword, sizeof(cfg.apPassword));
  cfg.coinPin = 0;
  cfg.relay1Pin = 5;
  cfg.relay2Pin = 4;
  cfg.buzzerPin = 3;
  cfg.displayClk = 12;
  cfg.displayDio = 13;
  cfg.coinActiveLow = true;
  cfg.relay1ActiveLow = true;
  cfg.relay2ActiveLow = false;
  cfg.coinMinutes = 1;
  cfg.coinSeconds = 0;
  cfg.warningSeconds = 10;
  cfg.displayBrightness = 7;
  cfg.remainingSeconds = 0;
  cfg.lifetimeCoins = 0;
  saveConfig();
}

void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, cfg);
  cfg.apName[sizeof(cfg.apName) - 1] = '\0';
  cfg.apPassword[sizeof(cfg.apPassword) - 1] = '\0';
  if (cfg.magic != EEPROM_MAGIC || strlen(cfg.apName) == 0 || strlen(cfg.apName) > 32 || !validApPassword(String(cfg.apPassword))) {
    loadDefaults();
  }
}

void applyPinModes() {
  pinMode(cfg.coinPin, cfg.coinActiveLow ? INPUT_PULLUP : INPUT);
  setRelays(cfg.remainingSeconds > 0);
  setupDisplay();
  showRemaining();
}

void redirectHome() {
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

uint32_t secondsPerCoin() {
  return (uint32_t)cfg.coinMinutes * 60UL + cfg.coinSeconds;
}

void acceptCoin() {
  uint32_t add = secondsPerCoin();
  if (add == 0) return;
  if (UINT32_MAX - cfg.remainingSeconds < add) cfg.remainingSeconds = UINT32_MAX;
  else cfg.remainingSeconds += add;
  cfg.lifetimeCoins++;
  sessionCoins++;
  timerRunning = cfg.remainingSeconds > 0;
  setRelays(timerRunning);
  beepCoin();
  saveConfig();
  showRemaining();
}

String pageHtml() {
  String h;
  h.reserve(9000);
  h += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  h += F("<title>Blaze Pisonet Timer</title><style>body{margin:0;background:#0b1118;color:#edf4fb;font:14px/1.5 system-ui,sans-serif}main{max-width:860px;margin:auto;padding:24px}.card{background:#131d28;border:1px solid #29394c;border-radius:14px;padding:16px;margin:12px 0}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}label{display:block;color:#9cafc3;font-size:12px;margin-bottom:4px}input,button{box-sizing:border-box;width:100%;padding:9px;border-radius:8px;border:1px solid #34475c;background:#0b141e;color:#fff}button{background:#1a5d95;cursor:pointer}.danger{background:#8d3140}.muted{color:#92a6b9}.kpi{font-size:32px;font-weight:900}@media(max-width:650px){.grid{grid-template-columns:1fr}}</style></head><body><main>");
  h += F("<h1>Blaze Pisonet Timer</h1><p class='muted'>Local timer control · public-safe continuation · no embedded media payloads</p>");
  h += F("<div class='card'><div class='kpi'>");
  char t[16];
  snprintf(t, sizeof(t), "%02lu:%02lu", (unsigned long)(cfg.remainingSeconds / 60UL), (unsigned long)(cfg.remainingSeconds % 60UL));
  h += t;
  h += F("</div><p class='muted'>Remaining paid time</p><div class='grid'><form method='post' action='/add'><button>Add one coin</button></form><form method='post' action='/reset'><button class='danger'>Reset timer</button></form></div></div>");
  h += F("<div class='card'><h2>Usage</h2><div class='grid'><div><b>Session coins</b><br>"); h += sessionCoins; h += F("</div><div><b>Lifetime coins</b><br>"); h += cfg.lifetimeCoins; h += F("</div></div></div>");
  h += F("<div class='card'><h2>Settings</h2><form method='post' action='/settings'><div class='grid'>");
  h += F("<div><label>AP name</label><input name='apName' maxlength='32' value='"); h += htmlEscape(String(cfg.apName)); h += F("'></div>");
  h += F("<div><label>New AP password</label><input name='apPassword' type='password' minlength='8' maxlength='63' placeholder='Leave blank to keep current'></div>");
  h += F("<div><label>Minutes per coin</label><input name='coinMinutes' type='number' min='0' max='180' value='"); h += cfg.coinMinutes; h += F("'></div>");
  h += F("<div><label>Extra seconds per coin</label><input name='coinSeconds' type='number' min='0' max='59' value='"); h += cfg.coinSeconds; h += F("'></div>");
  h += F("<div><label>Warning threshold (seconds)</label><input name='warningSeconds' type='number' min='0' max='120' value='"); h += cfg.warningSeconds; h += F("'></div>");
  h += F("<div><label>Display brightness (0-7)</label><input name='brightness' type='number' min='0' max='7' value='"); h += cfg.displayBrightness; h += F("'></div>");
  h += F("</div><p><button>Save settings</button></p></form><p class='muted'>Changing the AP name/password takes effect after restart. The existing password is never displayed by the web page.</p></div>");
  h += F("<div class='card'><h2>Hardware test</h2><form method='post' action='/test'><button>Test relay + tone</button></form></div>");
  h += F("</main></body></html>");
  return h;
}

void setupRoutes() {
  server.on("/", HTTP_GET, [](){ server.send(200, "text/html", pageHtml()); });

  server.on("/api/status", HTTP_GET, [](){
    String j = "{\"remainingSeconds\":" + String(cfg.remainingSeconds) +
               ",\"running\":" + String(timerRunning ? "true" : "false") +
               ",\"sessionCoins\":" + String(sessionCoins) +
               ",\"lifetimeCoins\":" + String(cfg.lifetimeCoins) + "}";
    server.send(200, "application/json", j);
  });

  server.on("/add", HTTP_POST, [](){ acceptCoin(); redirectHome(); });

  server.on("/reset", HTTP_POST, [](){
    cfg.remainingSeconds = 0;
    timerRunning = false;
    setRelays(false);
    noTone(cfg.buzzerPin);
    saveConfig();
    showRemaining();
    redirectHome();
  });

  server.on("/test", HTTP_POST, [](){
    setRelays(true); delay(120); setRelays(timerRunning);
    beep(1600, 120);
    redirectHome();
  });

  server.on("/settings", HTTP_POST, [](){
    if (server.hasArg("apName")) {
      String n = server.arg("apName"); n.trim();
      if (n.length() >= 1 && n.length() <= 32) n.toCharArray(cfg.apName, sizeof(cfg.apName));
    }
    if (server.hasArg("apPassword")) {
      String p = server.arg("apPassword");
      if (p.length() > 0 && validApPassword(p)) p.toCharArray(cfg.apPassword, sizeof(cfg.apPassword));
    }
    if (server.hasArg("coinMinutes")) cfg.coinMinutes = constrain(server.arg("coinMinutes").toInt(), 0, 180);
    if (server.hasArg("coinSeconds")) cfg.coinSeconds = constrain(server.arg("coinSeconds").toInt(), 0, 59);
    if (server.hasArg("warningSeconds")) cfg.warningSeconds = constrain(server.arg("warningSeconds").toInt(), 0, 120);
    if (server.hasArg("brightness")) cfg.displayBrightness = constrain(server.arg("brightness").toInt(), 0, 7);
    saveConfig();
    setupDisplay();
    showRemaining();
    redirectHome();
  });

  server.onNotFound([](){
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
}

void handleCoinInput() {
  bool active = digitalRead(cfg.coinPin) == (cfg.coinActiveLow ? LOW : HIGH);
  uint32_t now = millis();
  if (active && !coinLatched && now - lastCoinEdgeMs > 100) {
    coinLatched = true;
    lastCoinEdgeMs = now;
    acceptCoin();
  } else if (!active) {
    coinLatched = false;
  }
}

void updateTimer() {
  uint32_t now = millis();
  if (!timerRunning || cfg.remainingSeconds == 0) {
    timerRunning = false;
    setRelays(false);
    lastTickMs = now;
    return;
  }

  while (now - lastTickMs >= 1000UL && cfg.remainingSeconds > 0) {
    lastTickMs += 1000UL;
    cfg.remainingSeconds--;

    if (cfg.warningSeconds > 0 && cfg.remainingSeconds <= cfg.warningSeconds && cfg.remainingSeconds > 0) {
      beepWarning();
    }

    if (cfg.remainingSeconds == 0) {
      timerRunning = false;
      setRelays(false);
      beepTimeout();
      saveConfig();
    }
    showRemaining();
  }
}

void setup() {
  Serial.begin(115200);
  delay(30);
  loadConfig();
  applyPinModes();

  WiFi.mode(WIFI_AP);
  bool ok = strlen(cfg.apPassword) == 0 ? WiFi.softAP(cfg.apName) : WiFi.softAP(cfg.apName, cfg.apPassword);
  if (!ok) {
    defaultApName().toCharArray(cfg.apName, sizeof(cfg.apName));
    randomPassword().toCharArray(cfg.apPassword, sizeof(cfg.apPassword));
    saveConfig();
    WiFi.softAP(cfg.apName, cfg.apPassword);
  }

  timerRunning = cfg.remainingSeconds > 0;
  setRelays(timerRunning);
  lastTickMs = millis();

  setupRoutes();
  server.begin();

  Serial.println();
  Serial.println(F("Blaze Pisonet Timer public-safe continuation"));
  Serial.print(F("AP: ")); Serial.println(cfg.apName);
  Serial.print(F("AP password: ")); Serial.println(cfg.apPassword);
  Serial.print(F("Web UI: http://")); Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  handleCoinInput();
  updateTimer();
  yield();
}
