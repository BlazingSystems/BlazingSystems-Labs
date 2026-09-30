#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <TM1637Display.h>
#include <LittleFS.h>
#include <vector>
#include <WiFiUdp.h>

// Original public notification tones.
// These short sequences are intentionally generic and are not derived from
// commercial audio, songs, speech recordings, or third-party sound files.

static const uint16_t welcomeFreq[] PROGMEM = { 784, 988, 1175 };
static const uint16_t welcomeDur[]  PROGMEM = { 100, 100, 160 };
static const size_t welcomeCount = sizeof(welcomeFreq) / sizeof(welcomeFreq[0]);

static const uint16_t timeoutFreq[] PROGMEM = { 880, 660, 440, 0, 440 };
static const uint16_t timeoutDur[]  PROGMEM = { 150, 150, 250, 80, 320 };
static const size_t timeoutCount = sizeof(timeoutFreq) / sizeof(timeoutFreq[0]);

static const uint16_t alertFreq[] PROGMEM = { 1047, 1319 };
static const uint16_t alertDur[]  PROGMEM = { 100, 180 };
static const size_t alertCount = sizeof(alertFreq) / sizeof(alertFreq[0]);


// ------------------------------
// EEPROM + persistent storage
// ------------------------------
// Keep the existing EEPROM layout and saved timer/sales data. The buzzer pin is
// forced to GPIO14 separately so stale GPIO3 settings cannot return.
const uint32_t EEPROM_MAGIC = 0x5049534F;

struct PersistentState {
  uint32_t magic;
  char apName[33];
  char apPassword[65];

  uint8_t coinPin;
  uint8_t relayPin;
  uint8_t relay2Pin;
  uint8_t buzzerPin;
  uint8_t displayClk;
  uint8_t displayDio;

  bool coinActiveLow;
  bool relayActiveLow;
  bool relay2ActiveLow;
  bool buzzerActiveLow;

  uint16_t addMinutes;
  uint16_t addSeconds;
  uint16_t warningSeconds;
  uint16_t chimeDurationSeconds;
  uint8_t countdownSpeedPercent;

  uint32_t remainingSeconds;
  bool timerRunning;
  uint32_t salesOverall;
  uint32_t salesToday;
  uint32_t salesMonth;
  uint32_t salesDayKey;
  uint32_t salesMonthKey;
  uint8_t displayBrightness;
};

PersistentState state;


// ------------------------------
// Universal centralized-control extension (kept separate from legacy state)
// ------------------------------
enum DeviceRole : uint8_t { ROLE_STANDALONE=0, ROLE_MASTER=1, ROLE_SLAVE=2 };
enum NetworkMode : uint8_t { NET_AP=0, NET_STA=1 };
const uint32_t EXT_MAGIC = 0x424C5A32; // "BLZ2"
const uint16_t UDP_PORT = 4210;

struct ExtendedState {
  uint32_t magic;
  uint8_t role;
  uint8_t networkMode;
  char staSsid[33];
  char staPassword[65];
  char deviceName[25];
  uint16_t nodeId;
  uint8_t requestPin;
  bool requestActiveLow;
  uint16_t staTimeoutSeconds;
  uint16_t requestTimeoutSeconds;
  uint16_t coinSessionTimeoutSeconds;
  uint16_t heartbeatTimeoutSeconds;
  bool masterAutoDiscover;
  char masterIp[16];
};
ExtendedState ext;

DeviceRole deviceRole = ROLE_STANDALONE;
NetworkMode networkMode = NET_AP;
char STA_SSID[33] = "";
char STA_PASSWORD[65] = "";
char DEVICE_NAME[25] = "Pisonet-01";
uint16_t nodeId = 1;
uint8_t requestPin = 2; // D4/GPIO2; keep HIGH at boot; active-low button to GND
bool requestActiveLow = true;
uint16_t staTimeoutSeconds = 15;
uint16_t requestTimeoutSeconds = 30;
uint16_t coinSessionTimeoutSeconds = 20;
uint16_t heartbeatTimeoutSeconds = 15;
bool masterAutoDiscover = true;
char MASTER_IP[16] = "192.168.4.1";

WiFiUDP udp;
struct NodeRecord { uint16_t id; String name; IPAddress ip; uint32_t lastSeen; uint32_t today; uint32_t month; uint32_t overall; };
std::vector<NodeRecord> nodes;
uint16_t activeSlaveId = 0;
IPAddress activeSlaveIp;
uint32_t activeSessionLastActivity = 0;
uint32_t activeSessionCoins = 0;
bool requestPending = false;
uint32_t lastCreditToken = 0;
uint32_t requestStartedAt = 0;
uint32_t lastHelloAt = 0;
uint32_t lastHeartbeatAt = 0;
bool networkReady = false;
bool fallbackAp = false;
uint32_t displayOverrideUntil = 0;
String displayOverrideText;


void defaultsExtendedState() {
  memset(&ext, 0, sizeof(ext));
  ext.magic=EXT_MAGIC; ext.role=ROLE_STANDALONE; ext.networkMode=NET_AP;
  strcpy(ext.deviceName,"Pisonet-01"); ext.nodeId=1; ext.requestPin=2; ext.requestActiveLow=true;
  ext.staTimeoutSeconds=15; ext.requestTimeoutSeconds=30; ext.coinSessionTimeoutSeconds=20;
  ext.heartbeatTimeoutSeconds=15; ext.masterAutoDiscover=true; strcpy(ext.masterIp,"192.168.4.1");
}
void applyExtendedState() {
  deviceRole=(DeviceRole)constrain((int)ext.role,0,2); networkMode=(NetworkMode)constrain((int)ext.networkMode,0,1);
  strlcpy(STA_SSID,ext.staSsid,sizeof(STA_SSID)); strlcpy(STA_PASSWORD,ext.staPassword,sizeof(STA_PASSWORD));
  strlcpy(DEVICE_NAME,ext.deviceName,sizeof(DEVICE_NAME)); nodeId=ext.nodeId?ext.nodeId:1; requestPin=ext.requestPin;
  requestActiveLow=ext.requestActiveLow; staTimeoutSeconds=ext.staTimeoutSeconds?ext.staTimeoutSeconds:15;
  requestTimeoutSeconds=ext.requestTimeoutSeconds?ext.requestTimeoutSeconds:30; coinSessionTimeoutSeconds=ext.coinSessionTimeoutSeconds?ext.coinSessionTimeoutSeconds:20;
  heartbeatTimeoutSeconds=ext.heartbeatTimeoutSeconds?ext.heartbeatTimeoutSeconds:15; masterAutoDiscover=ext.masterAutoDiscover;
  strlcpy(MASTER_IP,ext.masterIp,sizeof(MASTER_IP));
}
void saveExtendedState() {
  ext.magic=EXT_MAGIC; ext.role=deviceRole; ext.networkMode=networkMode; strlcpy(ext.staSsid,STA_SSID,sizeof(ext.staSsid));
  strlcpy(ext.staPassword,STA_PASSWORD,sizeof(ext.staPassword)); strlcpy(ext.deviceName,DEVICE_NAME,sizeof(ext.deviceName)); ext.nodeId=nodeId;
  ext.requestPin=requestPin; ext.requestActiveLow=requestActiveLow; ext.staTimeoutSeconds=staTimeoutSeconds; ext.requestTimeoutSeconds=requestTimeoutSeconds;
  ext.coinSessionTimeoutSeconds=coinSessionTimeoutSeconds; ext.heartbeatTimeoutSeconds=heartbeatTimeoutSeconds; ext.masterAutoDiscover=masterAutoDiscover;
  strlcpy(ext.masterIp,MASTER_IP,sizeof(ext.masterIp)); EEPROM.put(sizeof(PersistentState),ext); EEPROM.commit();
}
void loadExtendedState() {
  EEPROM.get(sizeof(PersistentState),ext); if(ext.magic!=EXT_MAGIC){ defaultsExtendedState(); EEPROM.put(sizeof(PersistentState),ext); EEPROM.commit(); }
  applyExtendedState();
}

// ------------------------------
// Board defaults
// ------------------------------
char AP_NAME[33] = "BlazePisonet";
char AP_PASSWORD[65] = "";

ESP8266WebServer server(80);

// Default board mapping
uint8_t coinPin = 0;      // GPIO0 / FLASH button
uint8_t relayPin = 5;     // D1
uint8_t relay2Pin = 4;    // D2
const uint8_t FIXED_BUZZER_PIN = 14;  // D5, kept fixed for the speaker/buzzer module
uint8_t buzzerPin = FIXED_BUZZER_PIN;
uint8_t displayClk = 12;  // D6
uint8_t displayDio = 13;  // D7

bool coinActiveLow = true;
bool relayActiveLow = true;
bool relay2ActiveLow = false;
bool buzzerActiveLow = false;

// Timer config
uint16_t addMinutes = 1;
uint16_t addSeconds = 0;
uint16_t warningSeconds = 10;
uint16_t chimeDurationSeconds = 30;
uint8_t countdownSpeedPercent = 0;   // 0 = normal, max 50

// Runtime state
uint32_t remainingSeconds = 0;
bool timerRunning = false;
bool relayState = false;
bool relay2State = false;
bool coinLatch = false;

uint32_t lastCountdownMillis = 0;
float remainingSecondsFloat = 0.0f;

uint32_t buzzerUntil = 0;
uint32_t lastCoin = 0;

TM1637Display* timerDisplay = nullptr;
File uploadFile;
bool uploadSucceeded = false;
String uploadMessage = "No upload received";

// Non-blocking chime scheduler
enum class ChimeKind : uint8_t { None, Welcome, Coin, NearEnd, Timeout, Manual };
enum class ChimeSource : uint8_t { None, Ram, Progmem };

std::vector<int> chimeFreqs;
std::vector<int> chimeDurs;
const uint16_t* progmemChimeFreqs = nullptr;
const uint16_t* progmemChimeDurs = nullptr;
size_t progmemChimeCount = 0;
ChimeSource chimeSource = ChimeSource::None;
ChimeKind currentChime = ChimeKind::None;
uint32_t chimeStepStartedAt = 0;
uint32_t chimeStepDurationMs = 0;
bool chimePlaying = false;
size_t chimeIndex = 0;
uint32_t lastNearEndStartedAt = 0;

// sales counters
uint32_t salesOverall = 0;
uint32_t salesToday = 0;
uint32_t salesMonth = 0;
uint32_t salesDayKey = 0;
uint32_t salesMonthKey = 0;

// Forward declarations used before their definitions. Arduino usually
// auto-generates these for .ino files, but explicit declarations make the
// sketch safer to move between build systems.
void savePersistentState();
void updateBuzzer();
void stopChime();
void queueProgmemPattern(const uint16_t* freqs, const uint16_t* durs, size_t count, ChimeKind kind);

// ------------------------------
// Utilities
// ------------------------------
String formatTimeMMSS(uint32_t totalSeconds) {
  uint32_t mm = totalSeconds / 60;
  uint32_t ss = totalSeconds % 60;

  String s;
  if (mm < 10) s += "0";
  s += String(mm);
  s += ":";
  if (ss < 10) s += "0";
  s += String(ss);

  return s;
}

String htmlEscape(String input) {
  input.replace("&", "&amp;");
  input.replace("\"", "&quot;");
  input.replace("<", "&lt;");
  input.replace(">", "&gt;");
  return input;
}

// Validate AP credentials before storing them. ESP8266 softAP WPA passwords
// should be 8..63 characters. An empty password means an open AP.
bool apConfigIsValid(const String& name, const String& password) {
  if (name.length() < 1 || name.length() > 32) return false;
  if (password.length() == 0) return true;
  return password.length() >= 8 && password.length() <= 63;
}

bool pinSetIsUnique(uint8_t c, uint8_t r1, uint8_t r2, uint8_t bz, uint8_t clk, uint8_t dio) {
  uint8_t pins[] = { c, r1, r2, bz, clk, dio };
  for (uint8_t i = 0; i < 6; i++) {
    if (pins[i] > 16) return false;
    // GPIO6..GPIO11 are normally wired to the ESP8266 SPI flash.
    if (pins[i] >= 6 && pins[i] <= 11) return false;
    for (uint8_t j = i + 1; j < 6; j++) {
      if (pins[i] == pins[j]) return false;
    }
  }
  return true;
}

// ------------------------------
// EEPROM load/save
// ------------------------------
void applyStateToRuntime() {
  // copy from persistent struct to runtime vars
  // EEPROM may contain stale/corrupt bytes even when the magic matches.
  // Copy defensively and force NUL termination.
  strncpy(AP_NAME, state.apName, sizeof(AP_NAME) - 1);
  AP_NAME[sizeof(AP_NAME) - 1] = '\0';
  strncpy(AP_PASSWORD, state.apPassword, sizeof(AP_PASSWORD) - 1);
  AP_PASSWORD[sizeof(AP_PASSWORD) - 1] = '\0';

  coinPin = state.coinPin;
  relayPin = state.relayPin;
  relay2Pin = state.relay2Pin;
  buzzerPin = FIXED_BUZZER_PIN;
  displayClk = state.displayClk;
  displayDio = state.displayDio;

  coinActiveLow = state.coinActiveLow;
  relayActiveLow = state.relayActiveLow;
  relay2ActiveLow = state.relay2ActiveLow;
  buzzerActiveLow = state.buzzerActiveLow;

  addMinutes = state.addMinutes;
  addSeconds = state.addSeconds;
  warningSeconds = state.warningSeconds;
  chimeDurationSeconds = state.chimeDurationSeconds;
  countdownSpeedPercent = state.countdownSpeedPercent;

  remainingSeconds = state.remainingSeconds;
  timerRunning = state.timerRunning;
  salesOverall = state.salesOverall;
  salesToday = state.salesToday;
  salesMonth = state.salesMonth;
  salesDayKey = state.salesDayKey;
  salesMonthKey = state.salesMonthKey;
}

void loadDefaults() {
  // Use per-device first-boot credentials instead of one universal public password.
  // The generated password is deterministic for recovery and can be changed in Settings.
  snprintf(AP_NAME, sizeof(AP_NAME), "BlazePisonet-%06lX", (unsigned long)ESP.getChipId());
  snprintf(AP_PASSWORD, sizeof(AP_PASSWORD), "Blaze-%06lX", (unsigned long)ESP.getChipId());

  coinPin = 0;
  relayPin = 5;
  relay2Pin = 4;
  buzzerPin = FIXED_BUZZER_PIN;
  displayClk = 12;
  displayDio = 13;

  coinActiveLow = true;
  relayActiveLow = true;
  relay2ActiveLow = false;
  buzzerActiveLow = false;

  addMinutes = 1;
  addSeconds = 0;
  warningSeconds = 10;
  chimeDurationSeconds = 30;
  countdownSpeedPercent = 0;

  remainingSeconds = 0;
  timerRunning = false;

  salesOverall = 0;
  salesToday = 0;
  salesMonth = 0;
  salesDayKey = 0;
  salesMonthKey = 0;

  memset(&state, 0, sizeof(state));
  state.magic = EEPROM_MAGIC;
  strcpy(state.apName, AP_NAME);
  strcpy(state.apPassword, AP_PASSWORD);

  state.coinPin = coinPin;
  state.relayPin = relayPin;
  state.relay2Pin = relay2Pin;
  state.buzzerPin = FIXED_BUZZER_PIN;
  state.displayClk = displayClk;
  state.displayDio = displayDio;

  state.coinActiveLow = coinActiveLow;
  state.relayActiveLow = relayActiveLow;
  state.relay2ActiveLow = relay2ActiveLow;
  state.buzzerActiveLow = buzzerActiveLow;

  state.addMinutes = addMinutes;
  state.addSeconds = addSeconds;
  state.warningSeconds = warningSeconds;
  state.chimeDurationSeconds = chimeDurationSeconds;
  state.countdownSpeedPercent = countdownSpeedPercent;

  state.remainingSeconds = remainingSeconds;
  state.timerRunning = timerRunning;
  state.salesOverall = salesOverall;
  state.salesToday = salesToday;
  state.salesMonth = salesMonth;
  state.salesDayKey = salesDayKey;
  state.salesMonthKey = salesMonthKey;
}

void loadPersistentState() {
  EEPROM.begin(1024);
  EEPROM.get(0, state);

  if (state.magic != EEPROM_MAGIC) {
    loadDefaults();
    savePersistentState();
    return;
  }

  applyStateToRuntime();
}

void savePersistentState() {
  state.magic = EEPROM_MAGIC;
  strcpy(state.apName, AP_NAME);
  strcpy(state.apPassword, AP_PASSWORD);

  state.coinPin = coinPin;
  state.relayPin = relayPin;
  state.relay2Pin = relay2Pin;
  state.buzzerPin = FIXED_BUZZER_PIN;
  state.displayClk = displayClk;
  state.displayDio = displayDio;

  state.coinActiveLow = coinActiveLow;
  state.relayActiveLow = relayActiveLow;
  state.relay2ActiveLow = relay2ActiveLow;
  state.buzzerActiveLow = buzzerActiveLow;

  state.addMinutes = addMinutes;
  state.addSeconds = addSeconds;
  state.warningSeconds = warningSeconds;
  state.chimeDurationSeconds = chimeDurationSeconds;
  state.countdownSpeedPercent = countdownSpeedPercent;

  state.remainingSeconds = remainingSeconds;
  state.timerRunning = timerRunning;
  state.salesOverall = salesOverall;
  state.salesToday = salesToday;
  state.salesMonth = salesMonth;
  state.salesDayKey = salesDayKey;
  state.salesMonthKey = salesMonthKey;

  EEPROM.put(0, state);
  EEPROM.commit();
}

bool pinsAreValid() {
  return pinSetIsUnique(coinPin, relayPin, relay2Pin, buzzerPin, displayClk, displayDio);
}

void revertToDefaults() {
  // Recover only the hardware map. Do not destroy sales or a paid timer just
  // because a stored pin assignment became invalid.
  coinPin = 0;
  relayPin = 5;
  relay2Pin = 4;
  buzzerPin = FIXED_BUZZER_PIN;
  displayClk = 12;
  displayDio = 13;

  coinActiveLow = true;
  relayActiveLow = true;
  relay2ActiveLow = false;
  buzzerActiveLow = false;

  savePersistentState();
}


// ------------------------------
// Display
// ------------------------------
void setupDisplay() {
  if (timerDisplay) {
    delete timerDisplay;
    timerDisplay = nullptr;
  }

  timerDisplay = new TM1637Display(displayClk, displayDio);
  timerDisplay->setBrightness(7, true);
  timerDisplay->clear();
}

void showRemaining() {
  if (!timerDisplay) return;

  if (remainingSeconds == 0) {
    timerDisplay->showNumberDecEx(0, 0x40, true);
    return;
  }

  // A 4-digit TM1637 can only represent up to 99:59.
  uint32_t shown = remainingSeconds;
  if (shown > 5999UL) shown = 5999UL;

  uint16_t minutes = shown / 60;
  uint16_t seconds = shown % 60;
  uint16_t value = (minutes * 100) + seconds;
  timerDisplay->showNumberDecEx(value, 0x40, true);
}


// Seven-segment text is necessarily approximate. The four-digit display scrolls
// READY / INSERT COIN / BUSY and numeric node/coin status without blocking loop().
uint8_t segChar(char c){
  switch(toupper(c)){
    case '0': return 0x3F; case '1': return 0x06; case '2': return 0x5B; case '3': return 0x4F; case '4': return 0x66;
    case '5': return 0x6D; case '6': return 0x7D; case '7': return 0x07; case '8': return 0x7F; case '9': return 0x6F;
    case 'A': return 0x77; case 'B': return 0x7C; case 'C': return 0x39; case 'D': return 0x5E; case 'E': return 0x79;
    case 'F': return 0x71; case 'H': return 0x76; case 'I': return 0x06; case 'L': return 0x38; case 'N': return 0x54;
    case 'O': return 0x5C; case 'P': return 0x73; case 'R': return 0x50; case 'S': return 0x6D; case 'T': return 0x78;
    case 'U': return 0x3E; case 'Y': return 0x6E; case '-': return 0x40; default:return 0;
  }
}
void showText4(const String &t){ if(!timerDisplay)return; uint8_t d[4]={0,0,0,0}; for(uint8_t i=0;i<4 && i<t.length();i++)d[i]=segChar(t[i]); timerDisplay->setSegments(d); }
void showIpOnce(IPAddress ip){
  if(!timerDisplay)return; for(uint8_t q=0;q<4;q++){ timerDisplay->showNumberDec(ip[q],false,4,0); delay(650); yield(); }
}
void updateRoleDisplay(){
  if(!timerDisplay) return;
  uint32_t now=millis();
  if(displayOverrideUntil && (int32_t)(now-displayOverrideUntil)<0){
    String txt=displayOverrideText; if(txt.length()<=4){showText4(txt);return;} uint8_t span=txt.length()+4; uint8_t pos=(now/350)%span; String w="    "+txt+"    "; showText4(w.substring(pos,pos+4)); return;
  }
  if(deviceRole==ROLE_MASTER){
    if(activeSlaveId){ uint8_t phase=(now/1300)%2; if(!phase){ showText4("CS"); timerDisplay->showNumberDec(activeSlaveId,false,2,2); } else timerDisplay->showNumberDec(activeSessionCoins,false,4,0); }
    else { String w="    READY    "; uint8_t pos=(now/350)%(w.length()-3); showText4(w.substring(pos,pos+4)); }
  } else if(deviceRole==ROLE_SLAVE && requestPending){ String w="    INSERT COIN    "; uint8_t pos=(now/350)%(w.length()-3); showText4(w.substring(pos,pos+4)); }
  else showRemaining();
}
void flashDisplayMessage(const String &msg,uint32_t ms=1800){ displayOverrideText=msg; displayOverrideUntil=millis()+ms; }

// ------------------------------
// Relays + Buzzer
// ------------------------------
void setRelay1(bool on) {
  relayState = on;
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, (on ^ relayActiveLow) ? HIGH : LOW);
}

void setRelay2(bool on) {
  relay2State = on;
  pinMode(relay2Pin, OUTPUT);
  digitalWrite(relay2Pin, (on ^ relay2ActiveLow) ? HIGH : LOW);
}

void setRelays(bool on) {
  setRelay1(on);
  setRelay2(on);
}

void setBuzzer(bool on) {
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, (on ^ buzzerActiveLow) ? HIGH : LOW);
}

void noBuzzer() {
  noTone(buzzerPin);
  setBuzzer(false);
}

// ------------------------------
// Chime scheduler
// ------------------------------
void stopChime() {
  chimePlaying = false;
  chimeIndex = 0;
  chimeSource = ChimeSource::None;
  currentChime = ChimeKind::None;
  progmemChimeFreqs = nullptr;
  progmemChimeDurs = nullptr;
  progmemChimeCount = 0;
  noTone(buzzerPin);
  setBuzzer(false);
}

size_t activeChimeCount() {
  if (chimeSource == ChimeSource::Progmem) return progmemChimeCount;
  if (chimeSource == ChimeSource::Ram) return chimeFreqs.size();
  return 0;
}

uint16_t activeChimeFrequency(size_t index) {
  if (chimeSource == ChimeSource::Progmem) {
    return pgm_read_word(&progmemChimeFreqs[index]);
  }
  return (uint16_t)chimeFreqs[index];
}

uint16_t activeChimeDuration(size_t index) {
  if (chimeSource == ChimeSource::Progmem) {
    return pgm_read_word(&progmemChimeDurs[index]);
  }
  return (uint16_t)chimeDurs[index];
}

void beginCurrentChimeStep() {
  if (!chimePlaying || chimeIndex >= activeChimeCount()) {
    stopChime();
    return;
  }

  uint16_t frequency = activeChimeFrequency(chimeIndex);
  chimeStepDurationMs = activeChimeDuration(chimeIndex);
  chimeStepStartedAt = millis();

  if (frequency == 0) {
    noTone(buzzerPin);  // A zero-frequency entry is a deliberate rest.
    setBuzzer(false);
  } else {
    tone(buzzerPin, frequency, chimeStepDurationMs + 10UL);
  }
}

void queuePattern(const std::vector<int>& freqs, const std::vector<int>& durs,
                  ChimeKind kind = ChimeKind::Manual) {
  if (freqs.empty() || durs.empty() || freqs.size() != durs.size()) {
    stopChime();
    return;
  }

  stopChime();
  chimeFreqs.clear();
  chimeDurs.clear();

  // Uploaded/custom patterns retain the configurable safety limit.
  uint32_t maxDurationMs = (uint32_t)chimeDurationSeconds * 1000UL;
  uint32_t totalMs = 0;

  chimeFreqs.reserve(freqs.size());
  chimeDurs.reserve(durs.size());

  for (size_t i = 0; i < freqs.size(); i++) {
    if (totalMs >= maxDurationMs) break;

    int frequency = freqs[i];
    uint32_t duration = durs[i] > 0 ? (uint32_t)durs[i] : 0;
    if (frequency < 0 || frequency > 5000 || duration == 0) continue;
    if (totalMs + duration > maxDurationMs) duration = maxDurationMs - totalMs;
    if (duration == 0) break;

    chimeFreqs.push_back(frequency);
    chimeDurs.push_back((int)duration);
    totalMs += duration;
  }

  if (chimeFreqs.empty()) {
    stopChime();
    return;
  }

  chimeSource = ChimeSource::Ram;
  currentChime = kind;
  chimeIndex = 0;
  chimePlaying = true;
  beginCurrentChimeStep();
}

void queueProgmemPattern(const uint16_t* freqs, const uint16_t* durs,
                         size_t count, ChimeKind kind) {
  if (!freqs || !durs || count == 0) {
    stopChime();
    return;
  }

  stopChime();
  progmemChimeFreqs = freqs;
  progmemChimeDurs = durs;
  progmemChimeCount = count;
  chimeSource = ChimeSource::Progmem;
  currentChime = kind;
  chimeIndex = 0;
  chimePlaying = true;
  beginCurrentChimeStep();
}

void updateChimePlayer() {
  if (!chimePlaying) return;

  uint32_t now = millis();
  if ((uint32_t)(now - chimeStepStartedAt) < chimeStepDurationMs) return;

  chimeIndex++;
  if (chimeIndex >= activeChimeCount()) {
    stopChime();
    return;
  }

  beginCurrentChimeStep();
}

// ------------------------------
// Chime parser
// supports:
  // 1) note text format: frequency,duration;frequency,duration;...
  // 2) 8-bit mono WAV files: /welcome.wav, /coin.wav, /warning.wav, /timeup.wav
// ------------------------------
std::vector<int> parsedFreqs;
std::vector<int> parsedDurs;

bool parseNoteListFile(const String& path, std::vector<int>& outFreqs, std::vector<int>& outDurs) {
  outFreqs.clear();
  outDurs.clear();

  if (!LittleFS.exists(path)) return false;

  File f = LittleFS.open(path, "r");
  if (!f) return false;

  String data = f.readString();
  f.close();

  data.trim();
  if (data.length() == 0) return false;

  int start = 0;
  const int dataLength = (int)data.length();
  while (start < dataLength) {
    int end = data.indexOf(';', start);
    if (end < 0) end = dataLength;

    String token = data.substring(start, end);
    token.trim();

    if (token.length() > 0) {
      int comma = token.indexOf(',');
      if (comma > 0) {
        String freqStr = token.substring(0, comma);
        String durStr = token.substring(comma + 1);

        int freq = freqStr.toInt();
        int dur = durStr.toInt();

        if (freq >= 20 && freq <= 5000 && dur >= 10 && dur <= 10000) {
          outFreqs.push_back(freq);
          outDurs.push_back(dur);
          if (outFreqs.size() >= 300) break;
        }
      }
    }

    if (end >= dataLength) break;
    start = end + 1;
  }

  return outFreqs.size() > 0;
}

bool parseWave8BitMonoFile(const String& path, std::vector<int>& outFreqs, std::vector<int>& outDurs) {
  outFreqs.clear();
  outDurs.clear();

  if (!LittleFS.exists(path)) return false;

  File f = LittleFS.open(path, "r");
  if (!f) return false;

  char id[5] = {0};
  uint8_t tmp[4];

  if (f.readBytes(id, 4) != 4 || strncmp(id, "RIFF", 4) != 0) {
    f.close();
    return false;
  }

  if (f.readBytes((char*)tmp, 4) != 4) {
    f.close();
    return false;
  }

  char wave[5] = {0};
  if (f.readBytes(wave, 4) != 4 || strncmp(wave, "WAVE", 4) != 0) {
    f.close();
    return false;
  }

  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;
  uint32_t sampleRate = 0;
  uint16_t audioFormat = 0;
  uint16_t numChannels = 0;
  uint16_t bitsPerSample = 0;
  bool haveFmt = false;

  while (f.available() >= 8) {
    char chunkId[5] = {0};
    if (f.readBytes(chunkId, 4) != 4) break;
    if (f.readBytes((char*)tmp, 4) != 4) break;

    uint32_t chunkSize = ((uint32_t)tmp[3] << 24) |
                         ((uint32_t)tmp[2] << 16) |
                         ((uint32_t)tmp[1] << 8)  |
                         (uint32_t)tmp[0];

    uint32_t chunkDataStart = f.position();

    if (strncmp(chunkId, "fmt ", 4) == 0) {
      if (chunkSize < 16) {
        f.close();
        return false;
      }

      uint8_t fmtBuf[16];
      if (f.read(fmtBuf, 16) != 16) {
        f.close();
        return false;
      }

      audioFormat = (uint16_t)fmtBuf[0] | ((uint16_t)fmtBuf[1] << 8);
      numChannels = (uint16_t)fmtBuf[2] | ((uint16_t)fmtBuf[3] << 8);
      sampleRate = (uint32_t)fmtBuf[4] |
                   ((uint32_t)fmtBuf[5] << 8) |
                   ((uint32_t)fmtBuf[6] << 16) |
                   ((uint32_t)fmtBuf[7] << 24);
      bitsPerSample = (uint16_t)fmtBuf[14] | ((uint16_t)fmtBuf[15] << 8);
      haveFmt = true;
    } else if (strncmp(chunkId, "data", 4) == 0) {
      dataOffset = chunkDataStart;
      dataSize = chunkSize;
      break;
    }

    // RIFF chunks are padded to an even byte boundary. Seek to the end of
    // this chunk rather than adding chunkSize after already reading data.
    uint32_t nextChunk = chunkDataStart + chunkSize + (chunkSize & 1U);
    if (!f.seek(nextChunk, SeekSet)) {
      f.close();
      return false;
    }
  }

  f.close();

  if (!haveFmt || audioFormat != 1 || numChannels != 1 ||
      bitsPerSample != 8 || sampleRate == 0 || dataSize == 0) {
    return false;
  }

  File wav = LittleFS.open(path, "r");
  if (!wav) return false;

  const uint32_t maxSteps = 300;
  uint32_t step = (dataSize + maxSteps - 1) / maxSteps;
  if (step < 1) step = 1;

  uint32_t durationMs = (step * 1000UL + sampleRate / 2) / sampleRate;
  if (durationMs < 8) durationMs = 8;
  if (durationMs > 100) durationMs = 100;

  outFreqs.reserve(maxSteps);
  outDurs.reserve(maxSteps);

  for (uint32_t i = 0; i < dataSize && outFreqs.size() < maxSteps; i += step) {
    if (!wav.seek(dataOffset + i, SeekSet)) break;

    int sample = wav.read();
    if (sample < 0) break;

    int centered = sample - 128;
    int amplitude = abs(centered);

    // This is intentionally a buzzer-safe approximation, not WAV playback.
    // Stronger sample amplitudes are mapped to higher buzzer frequencies.
    int freq = 220 + amplitude * 12;
    if (freq > 1800) freq = 1800;

    outFreqs.push_back(freq);
    outDurs.push_back((int)durationMs);
  }

  wav.close();
  return !outFreqs.empty();
}

bool loadChimePattern(const String& path, std::vector<int>& outFreqs, std::vector<int>& outDurs) {
  if (path.endsWith(".wav")) {
    return parseWave8BitMonoFile(path, outFreqs, outDurs);
  }
  return parseNoteListFile(path, outFreqs, outDurs);
}

// ------------------------------
// More fun welcome / warning / near-ending tones
// ------------------------------
// Keep insert coin tone as-is (liked already)
void playCoinChime() {
  // Keep the original insert-coin chime exactly as requested.
  std::vector<int> freqs = { 880, 1180, 880, 1180 };
  std::vector<int> durs  = { 110, 150, 110, 180 };
  queuePattern(freqs, durs, ChimeKind::Coin);
}

void playWelcomeChime() {
  // Short original welcome chime.
  queueProgmemPattern(welcomeFreq, welcomeDur, welcomeCount, ChimeKind::Welcome);
}

void playWarningChime() {
  // Short original alert chime.
  queueProgmemPattern(alertFreq, alertDur, alertCount, ChimeKind::Manual);
}

void playNearEndingChime() {
  queueProgmemPattern(alertFreq, alertDur, alertCount, ChimeKind::NearEnd);
  lastNearEndStartedAt = millis();
}

void playTimeEndingChime() {
  // Plays the generic timeout sequence. It is interrupted only
  // when another chime is deliberately started, such as inserting a coin.
  queueProgmemPattern(timeoutFreq, timeoutDur, timeoutCount, ChimeKind::Timeout);
}

// ------------------------------
// Timer logic + countdown speed percent
// ------------------------------
float getCountdownScale() {
  return 1.0f + ((float)countdownSpeedPercent / 100.0f);
}

void startTimerFromSeconds(uint32_t secs) {
  if (secs == 0) {
    remainingSeconds = 0;
    timerRunning = false;
    remainingSecondsFloat = 0.0f;
    setRelays(false);
    showRemaining();
    savePersistentState();
    return;
  }

  // Any active timeout/welcome/near-end chime must stop when paid time starts.
  stopChime();
  remainingSecondsFloat = (float)secs;
  remainingSeconds = secs;
  timerRunning = true;
  lastCountdownMillis = millis();
  setRelays(true);
  savePersistentState();
}

void addCoinTime() {
  uint32_t now = millis();

  uint32_t addMs = ((uint32_t)addMinutes * 60000UL) + ((uint32_t)addSeconds * 1000UL);
  uint32_t addSecondsFromCoin = addMs / 1000UL;

  if (timerRunning) {
    remainingSecondsFloat += (float)addSecondsFromCoin;
    remainingSeconds = (uint32_t)remainingSecondsFloat;
  } else {
    remainingSecondsFloat = (float)addSecondsFromCoin;
    remainingSeconds = addSecondsFromCoin;
    timerRunning = true;
  }

  lastCountdownMillis = now;
  setRelays(true);
  lastCoin = now;
  lastNearEndStartedAt = now;

  // Coin always has priority and also stops the timeout chime immediately.
  stopChime();
  playCoinChime();
  savePersistentState();
}

void updateTimerState() {
  uint32_t now = millis();

  if (!timerRunning) {
    if (remainingSeconds != 0) {
      remainingSeconds = 0;
      remainingSecondsFloat = 0.0f;
      showRemaining();
    }
    // Do not call noTone() here. Chimes can legitimately play while idle.
    return;
  }

  float scale = getCountdownScale();

  // Unsigned subtraction is safe across millis() rollover.
  uint32_t elapsedMs = now - lastCountdownMillis;
  if (elapsedMs == 0) return;

  remainingSecondsFloat -= ((float)elapsedMs / 1000.0f) * scale;
  lastCountdownMillis = now;

  if (remainingSecondsFloat <= 0.0f) {
    remainingSecondsFloat = 0.0f;
    remainingSeconds = 0;
    timerRunning = false;
    setRelays(false);
    showRemaining();
    playTimeEndingChime();
    savePersistentState();
    return;
  }

  remainingSeconds = (uint32_t)remainingSecondsFloat;

  if (remainingSeconds <= warningSeconds && remainingSeconds > 0) {
    // Repeat the supplied near-end chime every two seconds. Never interrupt
    // the insert-coin sound or another currently playing pattern.
    if (!chimePlaying && (uint32_t)(now - lastNearEndStartedAt) >= 2000UL) {
      playNearEndingChime();
    }
  }

  showRemaining();
}

// reset timer to zero via button/web route
void resetTimer() {
  stopChime();
  remainingSeconds = 0;
  remainingSecondsFloat = 0.0f;
  timerRunning = false;
  setRelays(false);
  showRemaining();
  savePersistentState();
}

// sales tracking
void updateSalesBuckets() {
  uint32_t dayBucket = millis() / 86400000UL;
  uint32_t monthBucket = millis() / 2629800000UL;  // ~30.44 days approximation

  if (dayBucket != salesDayKey) {
    salesToday = 0;
    salesDayKey = dayBucket;
  }

  if (monthBucket != salesMonthKey) {
    salesMonth = 0;
    salesMonthKey = monthBucket;
  }
}

void incrementSales() {
  // Roll the buckets before counting this pulse so the first coin after a
  // boundary is not immediately erased.
  updateSalesBuckets();
  salesOverall++;
  salesToday++;
  salesMonth++;
  savePersistentState();
}

// clear sales
void clearSales() {
  salesOverall = 0;
  salesToday = 0;
  salesMonth = 0;
  salesDayKey = millis() / 86400000UL;
  salesMonthKey = millis() / 2629800000UL;
  savePersistentState();
}

void handleCoinInput() {
  pinMode(coinPin, coinActiveLow ? INPUT_PULLUP : INPUT);

  bool coinTriggered = digitalRead(coinPin) == (coinActiveLow ? LOW : HIGH);

  if (coinTriggered && !coinLatch) {
    coinLatch = true;
    if (deviceRole == ROLE_MASTER) {
      if (activeSlaveId != 0) {
        activeSessionCoins++; activeSessionLastActivity=millis(); incrementSales();
        String msg="CREDIT|"+String(activeSlaveId)+"|"+String(activeSessionCoins)+"|"+String(millis());
        udp.beginPacket(activeSlaveIp,UDP_PORT); udp.write((const uint8_t*)msg.c_str(),msg.length()); udp.endPacket();
      } else { flashDisplayMessage("NO CS"); }
    } else if (deviceRole == ROLE_STANDALONE) { addCoinTime(); incrementSales(); }
  } else if (!coinTriggered) {
    coinLatch = false;
  }
}


NodeRecord* findNode(uint16_t id){ for(auto &n:nodes)if(n.id==id)return &n; return nullptr; }
void touchNode(uint16_t id,const String& name,IPAddress ip){ NodeRecord*n=findNode(id); if(!n){nodes.push_back({id,name,ip,millis(),0,0,0});n=&nodes.back();} n->name=name;n->ip=ip;n->lastSeen=millis(); }
void sendUdp(IPAddress ip,const String&m){udp.beginPacket(ip,UDP_PORT);udp.write((const uint8_t*)m.c_str(),m.length());udp.endPacket();}
void sendToMaster(const String&m){IPAddress ip;if(ip.fromString(MASTER_IP))sendUdp(ip,m);}
void handleUdp(){
  int sz=udp.parsePacket(); if(!sz)return; char b[220];int n=udp.read(b,sizeof(b)-1);if(n<=0)return;b[n]=0;String m(b);IPAddress rip=udp.remoteIP();
  int p1=m.indexOf('|');String cmd=p1<0?m:m.substring(0,p1); std::vector<String>a; int st=p1+1; while(p1>=0){int nx=m.indexOf('|',st);a.push_back(nx<0?m.substring(st):m.substring(st,nx));if(nx<0)break;st=nx+1;}
  if(deviceRole==ROLE_MASTER){
    if(cmd=="HELLO" && a.size()>=2){uint16_t id=a[0].toInt();touchNode(id,a[1],rip);sendUdp(rip,"HELLO_ACK|"+String(id));}
    else if(cmd=="REQ" && a.size()>=2){uint16_t id=a[0].toInt();touchNode(id,a[1],rip); if(activeSlaveId==0){activeSlaveId=id;activeSlaveIp=rip;activeSessionCoins=0;activeSessionLastActivity=millis();sendUdp(rip,"GRANT|"+String(id));}else sendUdp(rip,"BUSY|"+String(id));}
    else if(cmd=="SALE" && a.size()>=4){uint16_t id=a[0].toInt();NodeRecord*x=findNode(id);if(x){x->today=a[1].toInt();x->month=a[2].toInt();x->overall=a[3].toInt();x->lastSeen=millis();}}
    else if(cmd=="DONE" && a.size()>=1 && a[0].toInt()==activeSlaveId){activeSlaveId=0;activeSessionCoins=0;}
  } else if(deviceRole==ROLE_SLAVE){
    if(cmd=="GRANT" && a.size() && a[0].toInt()==nodeId){requestPending=true;requestStartedAt=millis();flashDisplayMessage("COIN",900);}
    else if(cmd=="BUSY" && a.size() && a[0].toInt()==nodeId){requestPending=false;flashDisplayMessage("BUSY",1800);tone(buzzerPin,700,250);}
    else if(cmd=="CREDIT" && a.size()>=4 && a[0].toInt()==nodeId){uint32_t tok=a[3].toInt(); if(tok!=lastCreditToken){lastCreditToken=tok;addCoinTime();incrementSales();requestStartedAt=millis();} sendToMaster("SALE|"+String(nodeId)+"|"+String(salesToday)+"|"+String(salesMonth)+"|"+String(salesOverall));}
  }
}
void handleRequestButton(){
  if(deviceRole!=ROLE_SLAVE)return; pinMode(requestPin,requestActiveLow?INPUT_PULLUP:INPUT); static bool lat=false; bool hit=digitalRead(requestPin)==(requestActiveLow?LOW:HIGH);
  if(hit&&!lat){lat=true;if(!requestPending){sendToMaster("REQ|"+String(nodeId)+"|"+String(DEVICE_NAME));requestStartedAt=millis();flashDisplayMessage("WAIT",1000);}}
  else if(!hit)lat=false;
  if(requestPending && millis()-requestStartedAt>(uint32_t)requestTimeoutSeconds*1000UL){requestPending=false;sendToMaster("DONE|"+String(nodeId));flashDisplayMessage("END",1200);}
}
void maintainCentral(){
  uint32_t now=millis(); if(deviceRole==ROLE_SLAVE && now-lastHelloAt>5000){sendToMaster("HELLO|"+String(nodeId)+"|"+String(DEVICE_NAME));lastHelloAt=now;}
  if(deviceRole==ROLE_MASTER && activeSlaveId && now-activeSessionLastActivity>(uint32_t)coinSessionTimeoutSeconds*1000UL){sendUdp(activeSlaveIp,"DONE|"+String(activeSlaveId));activeSlaveId=0;activeSessionCoins=0;}
}
void startFallbackAP(){WiFi.disconnect();WiFi.mode(WIFI_AP);WiFi.softAP(AP_NAME,strlen(AP_PASSWORD)?AP_PASSWORD:nullptr);fallbackAp=true;networkReady=true;}
void startNetwork(){
  const char* joinSsid = (networkMode==NET_STA) ? STA_SSID : AP_NAME;
  const char* joinPass = (networkMode==NET_STA) ? STA_PASSWORD : AP_PASSWORD;
  bool shouldJoin = (deviceRole==ROLE_SLAVE) || (networkMode==NET_STA && strlen(STA_SSID));
  if(shouldJoin && strlen(joinSsid)){WiFi.mode(WIFI_STA);WiFi.begin(joinSsid,joinPass);uint32_t t=millis();while(WiFi.status()!=WL_CONNECTED && millis()-t<(uint32_t)staTimeoutSeconds*1000UL){delay(50);yield();}
    if(WiFi.status()==WL_CONNECTED){networkReady=true;fallbackAp=false;showIpOnce(WiFi.localIP());}
    else {showText4("Err");delay(900);startFallbackAP();showIpOnce(WiFi.softAPIP());}
  } else {startFallbackAP();showIpOnce(WiFi.softAPIP());}
  udp.begin(UDP_PORT);
}

// ------------------------------
// Web page
// ------------------------------
String page() {
  String s;
  s.reserve(19000);

  s += F(R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<meta name="theme-color" content="#0b1220">
<title>Blaze Pisonet Timer</title>

<style>
*{
  box-sizing:border-box;
}

:root{
  --bg:#0b1220;
  --bg2:#101a2b;
  --panel:#131f32;
  --panel2:#17263c;
  --border:#263851;
  --text:#edf4ff;
  --muted:#8ea3bd;
  --primary:#42d6a4;
  --primary-dark:#0b362b;
  --blue:#56a8ff;
  --red:#ff6470;
  --yellow:#ffc85a;
  --shadow:0 10px 30px rgba(0,0,0,.28);
}

html{
  scroll-behavior:smooth;
}

body{
  margin:0;
  background:
    radial-gradient(circle at top right,#172947 0,transparent 35%),
    var(--bg);
  color:var(--text);
  font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
  min-height:100vh;
}

button,
input,
select{
  font:inherit;
}

a{
  color:inherit;
}

.app{
  max-width:1050px;
  margin:auto;
  padding:18px;
}

/* HEADER */
.topbar{
  display:flex;
  justify-content:space-between;
  align-items:center;
  gap:15px;
  margin-bottom:18px;
}

.brand{
  display:flex;
  align-items:center;
  gap:12px;
}

.logo{
  width:46px;
  height:46px;
  border-radius:14px;
  background:linear-gradient(135deg,#42d6a4,#3999ff);
  display:flex;
  align-items:center;
  justify-content:center;
  font-weight:900;
  font-size:22px;
  color:#061710;
  box-shadow:0 6px 18px rgba(66,214,164,.25);
}

.brand h1{
  font-size:20px;
  margin:0;
}

.brand small{
  display:block;
  color:var(--muted);
  margin-top:2px;
}

.status-pill{
  display:flex;
  align-items:center;
  gap:7px;
  color:#bdebdc;
  background:rgba(66,214,164,.09);
  border:1px solid rgba(66,214,164,.25);
  padding:8px 12px;
  border-radius:999px;
  font-size:13px;
  white-space:nowrap;
}

.status-dot{
  width:8px;
  height:8px;
  border-radius:50%;
  background:var(--primary);
  box-shadow:0 0 10px var(--primary);
}

/* NAVIGATION */
.tabs{
  position:sticky;
  top:0;
  z-index:50;
  display:flex;
  gap:7px;
  padding:8px;
  margin-bottom:18px;
  border:1px solid var(--border);
  background:rgba(14,23,38,.92);
  backdrop-filter:blur(10px);
  border-radius:14px;
  overflow-x:auto;
  box-shadow:0 8px 25px rgba(0,0,0,.2);
}

.tab-btn{
  flex:1;
  min-width:105px;
  border:0;
  padding:11px 14px;
  border-radius:10px;
  cursor:pointer;
  background:transparent;
  color:var(--muted);
  transition:.2s;
  font-weight:600;
}

.tab-btn:hover{
  background:#1d2b41;
  color:white;
}

.tab-btn.active{
  background:var(--primary);
  color:#062219;
  box-shadow:0 5px 15px rgba(66,214,164,.18);
}

/* SECTIONS */
.tab-page{
  display:none;
}

.tab-page.active{
  display:block;
  animation:fade .18s ease-out;
}

@keyframes fade{
  from{opacity:.35;transform:translateY(4px);}
  to{opacity:1;transform:none;}
}

.grid{
  display:grid;
  grid-template-columns:repeat(2,minmax(0,1fr));
  gap:14px;
}

.grid3{
  display:grid;
  grid-template-columns:repeat(3,minmax(0,1fr));
  gap:14px;
}

.card{
  background:linear-gradient(145deg,var(--panel),#101a2a);
  border:1px solid var(--border);
  border-radius:17px;
  padding:18px;
  box-shadow:var(--shadow);
}

.card-title{
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:10px;
  margin-bottom:14px;
}

.card h2{
  font-size:17px;
  margin:0;
}

.card p{
  color:var(--muted);
  margin:8px 0;
}

/* TIMER */
.timer-card{
  text-align:center;
  padding:28px 18px;
}

.timer-label{
  color:var(--muted);
  font-size:13px;
  text-transform:uppercase;
  letter-spacing:1.5px;
}

.timer{
  margin:7px 0 3px;
  font-size:64px;
  line-height:1.1;
  font-weight:800;
  letter-spacing:3px;
  font-variant-numeric:tabular-nums;
  background:linear-gradient(90deg,#fff,#71eac1);
  -webkit-background-clip:text;
  color:transparent;
}

.timer-sub{
  color:var(--muted);
  font-size:13px;
}

/* RELAYS */
.output-row{
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:14px;
  padding:13px 0;
  border-bottom:1px solid rgba(255,255,255,.055);
}

.output-row:last-child{
  border-bottom:0;
}

.output-info{
  min-width:0;
}

.output-info strong{
  display:block;
  margin-bottom:3px;
}

.output-info small{
  color:var(--muted);
}

.badge{
  display:inline-flex;
  align-items:center;
  gap:6px;
  border-radius:999px;
  padding:5px 9px;
  font-size:12px;
  font-weight:700;
}

.badge.on{
  color:#6af1c1;
  background:rgba(66,214,164,.12);
  border:1px solid rgba(66,214,164,.25);
}

.badge.off{
  color:#a6b6ca;
  background:#1d2a3e;
  border:1px solid #30435e;
}

.dot{
  width:7px;
  height:7px;
  border-radius:50%;
  background:currentColor;
}

/* BUTTONS */
.btn{
  display:inline-flex;
  align-items:center;
  justify-content:center;
  gap:7px;
  width:100%;
  text-decoration:none;
  border:0;
  border-radius:10px;
  padding:11px 14px;
  cursor:pointer;
  font-weight:700;
  transition:.18s;
}

.btn:hover{
  transform:translateY(-1px);
  filter:brightness(1.06);
}

.btn-primary{
  background:var(--primary);
  color:#06261c;
}

.btn-secondary{
  background:#243650;
  color:#dbe9fa;
}

.btn-danger{
  background:rgba(255,100,112,.14);
  color:#ff9da5;
  border:1px solid rgba(255,100,112,.28);
}

.btn-warning{
  background:rgba(255,200,90,.13);
  color:#ffd77d;
  border:1px solid rgba(255,200,90,.28);
}

.actions{
  display:grid;
  grid-template-columns:repeat(2,1fr);
  gap:9px;
  margin-top:13px;
}

/* FORMS */
.group-title{
  margin:22px 0 10px;
  color:#77e2be;
  font-size:12px;
  letter-spacing:1.3px;
  text-transform:uppercase;
  font-weight:800;
}

.group-title:first-child{
  margin-top:0;
}

.form-grid{
  display:grid;
  grid-template-columns:repeat(2,minmax(0,1fr));
  gap:14px;
}

.field{
  min-width:0;
}

.field.full{
  grid-column:1/-1;
}

label{
  display:block;
  color:#b9c9dc;
  margin-bottom:7px;
  font-size:13px;
  font-weight:600;
}

input,
select{
  width:100%;
  padding:11px 12px;
  border-radius:10px;
  border:1px solid #2c415e;
  background:#0c1625;
  color:#f2f7ff;
  outline:none;
  transition:.18s;
}

input:focus,
select:focus{
  border-color:var(--primary);
  box-shadow:0 0 0 3px rgba(66,214,164,.10);
}

input[type="file"]{
  padding:9px;
}

.help{
  color:#738ba7;
  font-size:11px;
  margin-top:5px;
}

/* SWITCH */
.toggle-row{
  display:flex;
  justify-content:space-between;
  align-items:center;
  gap:12px;
  padding:12px 0;
  border-bottom:1px solid rgba(255,255,255,.05);
}

.toggle-row:last-child{
  border-bottom:0;
}

.toggle-row span{
  font-size:14px;
}

.switch{
  position:relative;
  width:46px;
  height:26px;
  flex:none;
}

.switch input{
  display:none;
}

.slider{
  position:absolute;
  inset:0;
  cursor:pointer;
  background:#33445b;
  border-radius:999px;
  transition:.2s;
}

.slider:before{
  content:"";
  position:absolute;
  width:20px;
  height:20px;
  top:3px;
  left:3px;
  border-radius:50%;
  background:white;
  transition:.2s;
}

.switch input:checked + .slider{
  background:var(--primary);
}

.switch input:checked + .slider:before{
  transform:translateX(20px);
}

/* SALES */
.stat{
  background:#0e1929;
  border:1px solid var(--border);
  padding:18px;
  border-radius:14px;
}

.stat span{
  color:var(--muted);
  font-size:12px;
  text-transform:uppercase;
  letter-spacing:1px;
}

.stat strong{
  display:block;
  margin-top:5px;
  font-size:28px;
}

/* INFO */
.info-table{
  width:100%;
  border-collapse:collapse;
}

.info-table td{
  padding:11px 4px;
  border-bottom:1px solid rgba(255,255,255,.05);
  vertical-align:top;
}

.info-table tr:last-child td{
  border-bottom:0;
}

.info-table td:first-child{
  color:var(--muted);
  width:40%;
}

.code{
  font-family:monospace;
  background:#0b1523;
  border:1px solid #243852;
  padding:2px 6px;
  border-radius:5px;
  color:#8ee9c9;
}

/* FOOTER */
.footer{
  text-align:center;
  color:#647a95;
  font-size:12px;
  padding:28px 10px 12px;
}

@media(max-width:700px){
  .app{
    padding:12px;
  }

  .grid,
  .grid3,
  .form-grid{
    grid-template-columns:1fr;
  }

  .timer{
    font-size:52px;
  }

  .topbar{
    align-items:flex-start;
  }

  .brand h1{
    font-size:17px;
  }

  .status-pill{
    padding:7px 9px;
    font-size:11px;
  }
}

@media(max-width:430px){
  .timer{
    font-size:43px;
  }

  .actions{
    grid-template-columns:1fr;
  }
}

.settings-shell{display:grid;grid-template-columns:190px 1fr;gap:14px}.settings-side{display:flex;flex-direction:column;gap:7px}.sub-btn{border:1px solid #26364d;background:#111d30;color:#cbd5e1;padding:11px;border-radius:9px;text-align:left}.sub-btn.active{background:#1d4ed8;color:white}.sub-page{display:none}.sub-page.active{display:block}@media(max-width:650px){.settings-shell{grid-template-columns:1fr}.settings-side{display:grid;grid-template-columns:repeat(2,1fr)}}
</style>
</head>

<body>
<div class="app">

<header class="topbar">
  <div class="brand">
    <div class="logo">B</div>
    <div>
      <h1>BLAZE Pisonet Timer</h1>
      <small>ESP8266 Control Panel</small>
    </div>
  </div>

  <div class="status-pill">
    <span class="status-dot"></span>
    ONLINE
  </div>
</header>

<nav class="tabs">
  <button class="tab-btn active" onclick="showTab('dashboard',this)">Dashboard</button>
  <button class="tab-btn" onclick="showTab('settings',this)">Settings</button>
  <button class="tab-btn" onclick="showTab('sales',this)">Sales</button>
  <button class="tab-btn" onclick="showTab('audio',this)">Chimes</button>
  <button class="tab-btn" onclick="showTab('info',this)">Info</button>
</nav>

<section id="dashboard" class="tab-page active">

  <div class="card timer-card">
    <div class="timer-label">Current remaining time</div>
    <div class="timer">
)rawliteral");

  s += formatTimeMMSS(remainingSeconds);

  s += F(R"rawliteral(
    </div>
    <div class="timer-sub">MM : SS</div>
  </div>

  <div class="grid">

    <div class="card">
      <div class="card-title">
        <h2>Output Control</h2>
      </div>

      <div class="output-row">
        <div class="output-info">
          <strong>Relay 1</strong>
          <small>Main computer power · D1</small>
        </div>
)rawliteral");

  if (relayState) {
    s += F("<span class='badge on'><span class='dot'></span>ON</span>");
  } else {
    s += F("<span class='badge off'><span class='dot'></span>OFF</span>");
  }

  s += F(R"rawliteral(
      </div>

      <a class="btn btn-secondary" href="/relay1">
)rawliteral");

  s += relayState ? "Turn Relay 1 OFF" : "Turn Relay 1 ON";

  s += F(R"rawliteral(
      </a>

      <div class="output-row">
        <div class="output-info">
          <strong>Relay 2</strong>
          <small>Secondary output · D2</small>
        </div>
)rawliteral");

  if (relay2State) {
    s += F("<span class='badge on'><span class='dot'></span>ON</span>");
  } else {
    s += F("<span class='badge off'><span class='dot'></span>OFF</span>");
  }

  s += F(R"rawliteral(
      </div>

      <a class="btn btn-secondary" href="/relay2">
)rawliteral");

  s += relay2State ? "Turn Relay 2 OFF" : "Turn Relay 2 ON";

  s += F(R"rawliteral(
      </a>
    </div>

    <div class="card">
      <div class="card-title">
        <h2>Timer Actions</h2>
      </div>

      <p>Manual timer and notification controls.</p>

      <div class="actions">
        <a class="btn btn-warning" href="/warn">
          Test Warning
        </a>

        <a class="btn btn-danger"
           href="/resetTimer"
           onclick="return confirm('Reset remaining timer to 00:00?')">
          Reset Timer
        </a>
      </div>
    </div>

  </div>
</section>

<section id="settings" class="tab-page">

<div class="card">
  <div class="card-title">
    <div>
      <h2>Device Configuration</h2>
      <p>Network, timing, hardware and polarity settings.</p>
    </div>
  </div>

  <form action="/settings" method="POST">
    <input type="hidden" name="save" value="1">
    <div class="settings-shell"><aside class="settings-side">
      <button type="button" class="sub-btn active" onclick="subTab('general',this)">General</button>
      <button type="button" class="sub-btn" onclick="subTab('central',this)">Centralized</button>
      <button type="button" class="sub-btn" onclick="subTab('network',this)">Network</button>
      <button type="button" class="sub-btn" onclick="subTab('hardware',this)">Hardware</button>
    </aside><div>
    <div id="sub-central" class="sub-page"><div class="group-title">Device Role & Centralized Control</div><div class="form-grid">
      <div class="field"><label>Operating Role</label><select name="role"><option value="0" %ROLE0%>Standalone Timer</option><option value="1" %ROLE1%>Master CS</option><option value="2" %ROLE2%>Slave Control</option></select></div>
      <div class="field"><label>Node ID</label><input name="nodeId" type="number" min="1" max="65535" value="%NODEID%"></div>
      <div class="field"><label>Device Name</label><input name="deviceName" maxlength="24" value="%DEVNAME%"></div>
      <div class="field"><label>Master IP</label><input name="masterIp" value="%MASTERIP%"><div class="help">Slave destination on the same offline LAN.</div></div>
      <div class="field"><label>Request Button GPIO</label><input name="requestPin" type="number" min="0" max="16" value="%REQPIN%"></div>
      <div class="field"><label>Request Timeout (s)</label><input name="requestTimeout" type="number" value="%REQTO%"></div>
      <div class="field"><label>Coin Session Timeout (s)</label><input name="coinSessionTimeout" type="number" value="%CSTO%"></div>
      <div class="field"><label>Heartbeat Timeout (s)</label><input name="heartbeatTimeout" type="number" value="%HBTO%"></div>
    </div><div class="toggle-row"><span>Request Button Active Low</span><label class="switch"><input name="requestActiveLow" type="checkbox" %REQLOW%><span class="slider"></span></label></div>
    <div class="toggle-row"><span>Master Auto Discover Nodes</span><label class="switch"><input name="masterAutoDiscover" type="checkbox" %AUTODISC%><span class="slider"></span></label></div></div>
    <div id="sub-network" class="sub-page"><div class="group-title">Offline Network</div><div class="form-grid">
      <div class="field"><label>Network Mode</label><select name="networkMode"><option value="0" %NET0%>AP / Recovery</option><option value="1" %NET1%>STA / Shared LAN</option></select></div>
      <div class="field"><label>STA SSID</label><input name="staSsid" maxlength="32" value="%STASSID%"></div>
      <div class="field"><label>STA Password</label><input name="staPassword" type="password" autocomplete="new-password" maxlength="63" value="%STAPASS%"></div>
      <div class="field"><label>STA Timeout (s)</label><input name="staTimeout" type="number" min="3" max="120" value="%STATO%"></div>
    </div><p class="help">No Internet is required. If STA fails, this board falls back to its configured AP so management remains reachable.</p></div>
    <div id="sub-hardware" class="sub-page"><p class="help">Hardware and polarity options remain below in General to preserve the original form behavior.</p></div>
    <div id="sub-general" class="sub-page active">
    <div class="group-title">Wi-Fi Access Point</div>

    <div class="form-grid">

      <div class="field">
        <label>Access Point Name</label>
        <input name="apName" type="text" value=")rawliteral");

  s += htmlEscape(String(AP_NAME));

  s += F(R"rawliteral(">
        <div class="help">Wi-Fi network broadcast by the timer.</div>
      </div>

      <div class="field">
        <label>Access Point Password</label>
        <input name="apPassword" type="password" autocomplete="new-password" value=")rawliteral");

  s += htmlEscape(String(AP_PASSWORD));

  s += F(R"rawliteral(">
        <div class="help">Use 8-63 characters, or leave blank for an open AP. Wi-Fi name/password changes apply after restart.</div>
      </div>

    </div>

    <div class="group-title">Coin & Timer</div>

    <div class="form-grid">

      <div class="field">
        <label>Minutes per Coin Pulse</label>
        <input name="addMinutes"
               type="number"
               min="0"
               max="180"
               value=")rawliteral");

  s += String(addMinutes);

  s += F(R"rawliteral(">
      </div>

      <div class="field">
        <label>Additional Seconds</label>
        <input name="addSeconds"
               type="number"
               min="0"
               max="59"
               value=")rawliteral");

  s += String(addSeconds);

  s += F(R"rawliteral(">
      </div>

      <div class="field">
        <label>Warning Time</label>
        <input name="warningSeconds"
               type="number"
               min="0"
               max="120"
               value=")rawliteral");

  s += String(warningSeconds);

  s += F(R"rawliteral(">
        <div class="help">Seconds before timer expiry.</div>
      </div>

      <div class="field">
        <label>Chime Duration</label>
        <input name="chimeDurationSeconds"
               type="number"
               min="1"
               max="120"
               value=")rawliteral");

  s += String(chimeDurationSeconds);

  s += F(R"rawliteral(">
      </div>

      <div class="field full">
        <label>Countdown Speed Adjustment (%)</label>
        <input name="speedPct"
               type="number"
               min="0"
               max="50"
               value=")rawliteral");

  s += String(countdownSpeedPercent);

  s += F(R"rawliteral(">
        <div class="help">
          0% = normal real-time countdown. 50% = countdown runs 50% faster.
        </div>
      </div>

    </div>

    <div class="group-title">Hardware Pin Mapping</div>
    <p class="help">Do not use GPIO6-GPIO11; they are reserved for the ESP8266 flash. GPIO0, GPIO2 and GPIO15 are boot-strap pins and should be used carefully.</p>

    <div class="form-grid">

      <div class="field">
        <label>Coin Input Pin</label>
        <input name="coinPin" type="number" min="0" max="16" value=")rawliteral");
  s += String(coinPin);

  s += F(R"rawliteral(">
      </div>

      <div class="field">
        <label>Relay 1 Pin</label>
        <input name="relayPin" type="number" min="0" max="16" value=")rawliteral");
  s += String(relayPin);

  s += F(R"rawliteral(">
      </div>

      <div class="field">
        <label>Relay 2 Pin</label>
        <input name="relay2Pin" type="number" min="0" max="16" value=")rawliteral");
  s += String(relay2Pin);

  s += F(R"rawliteral(">
      </div>

      <div class="field">
        <label>Buzzer Pin (fixed)</label>
        <input name="buzzerPin" type="number" value=")rawliteral");
  s += String(FIXED_BUZZER_PIN);

  s += F(R"rawliteral(" readonly>
      </div>

      <div class="field">
        <label>TM1637 CLK Pin</label>
        <input name="displayClk" type="number" min="0" max="16" value=")rawliteral");
  s += String(displayClk);

  s += F(R"rawliteral(">
      </div>

      <div class="field">
        <label>TM1637 DIO Pin</label>
        <input name="displayDio" type="number" min="0" max="16" value=")rawliteral");
  s += String(displayDio);

  s += F(R"rawliteral(">
      </div>

    </div>

    <div class="group-title">Signal Polarity</div>

    <div class="toggle-row">
      <span>Coin Input Active Low</span>
      <label class="switch">
        <input name="coinActiveLow" type="checkbox" )rawliteral");

  if (coinActiveLow) s += F("checked");

  s += F(R"rawliteral(>
        <span class="slider"></span>
      </label>
    </div>

    <div class="toggle-row">
      <span>Relay 1 Active Low</span>
      <label class="switch">
        <input name="relayActiveLow" type="checkbox" )rawliteral");

  if (relayActiveLow) s += F("checked");

  s += F(R"rawliteral(>
        <span class="slider"></span>
      </label>
    </div>

    <div class="toggle-row">
      <span>Relay 2 Active Low</span>
      <label class="switch">
        <input name="relay2ActiveLow" type="checkbox" )rawliteral");

  if (relay2ActiveLow) s += F("checked");

  s += F(R"rawliteral(>
        <span class="slider"></span>
      </label>
    </div>

    <div class="toggle-row">
      <span>Buzzer Active Low</span>
      <label class="switch">
        <input name="buzzerActiveLow" type="checkbox" )rawliteral");

  if (buzzerActiveLow) s += F("checked");

  s += F(R"rawliteral(>
        <span class="slider"></span>
      </label>
    </div>

    </div></div></div>
    <br>

    <button class="btn btn-primary" type="submit">
      Save Configuration
    </button>

  </form>
</div>
</section>

<section id="sales" class="tab-page">

<div class="card">
  <div class="card-title">
    <div>
      <h2>Sales Monitoring</h2>
      <p>Local totals plus discovered per-unit totals when this board is Master CS.</p>
      <p>Recorded coin pulse totals. Today/month buckets are uptime-based unless you add a real clock or network time source.</p>
    </div>
  </div>

  <div class="grid3">

    <div class="stat">
      <span>Today</span>
      <strong>
)rawliteral");

  s += String(salesToday);

  s += F(R"rawliteral(
      </strong>
    </div>

    <div class="stat">
      <span>This Month</span>
      <strong>
)rawliteral");

  s += String(salesMonth);

  s += F(R"rawliteral(
      </strong>
    </div>

    <div class="stat">
      <span>Overall</span>
      <strong>
)rawliteral");

  s += String(salesOverall);

  s += F(R"rawliteral(
      </strong>
    </div>

  </div>

  <br>

  <a class="btn btn-danger"
     href="/clearSales"
     onclick="return confirm('Clear all recorded sales data? This cannot be undone.')">
     Clear Sales Records
  </a>

</div>
</section>

<div class="card"><div class="card-title"><h2>Master Unit Monitor</h2></div><div style="overflow:auto"><table style="width:100%;border-collapse:collapse"><tr><th>ID</th><th>Name</th><th>IP</th><th>Status</th><th>Today</th><th>Month</th><th>Overall</th></tr>)rawliteral");
  if(deviceRole==ROLE_MASTER){ for(auto &n:nodes){ s += "<tr><td>"+String(n.id)+"</td><td>"+htmlEscape(n.name)+"</td><td>"+n.ip.toString()+"</td><td>"+String((millis()-n.lastSeen <= (uint32_t)heartbeatTimeoutSeconds*1000UL)?"ONLINE":"OFFLINE")+"</td><td>"+String(n.today)+"</td><td>"+String(n.month)+"</td><td>"+String(n.overall)+"</td></tr>"; }} else s += "<tr><td colspan='7'>Available in Master CS role.</td></tr>";
  s += F(R"rawliteral(</table></div></div>
<section id="audio" class="tab-page">

<div class="card">
  <div class="card-title">
    <div>
      <h2>Custom Chimes</h2>
      <p>Upload buzzer patterns or supported WAV files.</p>
    </div>
  </div>

  <form method="POST"
        action="/upload"
        enctype="multipart/form-data">

    <div class="form-grid">

      <div class="field">
        <label>Chime Type</label>
        <select name="patternName">
          <option value="welcome">Welcome Chime</option>
          <option value="coin">Coin Insert Chime</option>
          <option value="warning">Timer Warning</option>
          <option value="timeup">Time Up Chime</option>
        </select>
      </div>

      <div class="field">
        <label>Chime File</label>
        <input type="file"
               name="file"
               accept=".txt,.wav">
      </div>

    </div>

    <br>

    <button class="btn btn-primary" type="submit">
      Upload Chime
    </button>

  </form>

  <div class="group-title">Supported Formats</div>

  <p>
    Pattern:
    <span class="code">frequency,duration;</span>
  </p>

  <p>
    Example:
    <span class="code">1000,100;1500,100;2000,250;</span>
  </p>

  <p>
    WAV files should use <b>8-bit PCM mono</b> for conversion to a buzzer-safe pattern.
  </p>

</div>
</section>

<section id="info" class="tab-page">

<div class="card">

  <div class="card-title">
    <div>
      <h2>System Information</h2>
      <p>Current device configuration and usage information.</p>
    </div>
  </div>

  <table class="info-table">

    <tr>
      <td>Web Interface</td>
      <td><span class="code">192.168.4.1</span></td>
    </tr>

    <tr>
      <td>Controller</td>
      <td>ESP8266</td>
    </tr>

    <tr>
      <td>Default Coin Input</td>
      <td>D3</td>
    </tr>

    <tr>
      <td>Default Relay 1</td>
      <td>D1</td>
    </tr>

    <tr>
      <td>Default Relay 2</td>
      <td>D2</td>
    </tr>

    <tr>
      <td>Default Buzzer</td>
      <td>D5 / GPIO14 (fixed)</td>
    </tr>

    <tr>
      <td>TM1637 Display</td>
      <td>D6 CLK / D7 DIO</td>
    </tr>

    <tr>
      <td>Idle Display</td>
      <td><span class="code">00:00</span></td>
    </tr>

    <tr>
      <td>Timer Display</td>
      <td>MM:SS</td>
    </tr>

    <tr>
      <td>Countdown Speed</td>
      <td>0% = real-time</td>
    </tr>

    <tr>
      <td>AP Config Changes</td>
      <td>Applied after restart</td>
    </tr>

    <tr>
      <td>Pattern Files</td>
      <td>
        welcome.txt<br>
        coin.txt<br>
        warning.txt<br>
        timeup.txt
      </td>
    </tr>

  </table>

</div>
</section>

<footer class="footer">
  BLAZE Pisonet Timer Board · Local Embedded Control Interface
</footer>

</div>

<script>
function showTab(id,button){
  document.querySelectorAll('.tab-page').forEach(function(el){
    el.classList.remove('active');
  });

  document.querySelectorAll('.tab-btn').forEach(function(el){
    el.classList.remove('active');
  });

  document.getElementById(id).classList.add('active');

  if(button){
    button.classList.add('active');
  }

  try{
    localStorage.setItem('blazeTab',id);
  }catch(e){}
}

window.addEventListener('load',function(){
  try{
    var saved=localStorage.getItem('blazeTab');

    if(saved && document.getElementById(saved)){
      var buttons=document.querySelectorAll('.tab-btn');

      document.querySelectorAll('.tab-page').forEach(function(el){
        el.classList.remove('active');
      });

      buttons.forEach(function(el){
        el.classList.remove('active');
      });

      document.getElementById(saved).classList.add('active');

      var names=['dashboard','settings','sales','audio','info'];
      var index=names.indexOf(saved);

      if(index>=0 && buttons[index]){
        buttons[index].classList.add('active');
      }
    }
  }catch(e){}
});
</script>

<script>function subTab(name,btn){document.querySelectorAll('.sub-page').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.sub-btn').forEach(x=>x.classList.remove('active'));var p=document.getElementById('sub-'+name);if(p)p.classList.add('active');if(btn)btn.classList.add('active');}</script></body>
</html>
)rawliteral");

  s.replace("%ROLE0%",deviceRole==ROLE_STANDALONE?"selected":""); s.replace("%ROLE1%",deviceRole==ROLE_MASTER?"selected":""); s.replace("%ROLE2%",deviceRole==ROLE_SLAVE?"selected":"");
  s.replace("%NODEID%",String(nodeId)); s.replace("%DEVNAME%",htmlEscape(String(DEVICE_NAME))); s.replace("%MASTERIP%",htmlEscape(String(MASTER_IP)));
  s.replace("%REQPIN%",String(requestPin)); s.replace("%REQTO%",String(requestTimeoutSeconds)); s.replace("%CSTO%",String(coinSessionTimeoutSeconds)); s.replace("%HBTO%",String(heartbeatTimeoutSeconds));
  s.replace("%REQLOW%",requestActiveLow?"checked":""); s.replace("%AUTODISC%",masterAutoDiscover?"checked":"");
  s.replace("%NET0%",networkMode==NET_AP?"selected":""); s.replace("%NET1%",networkMode==NET_STA?"selected":""); s.replace("%STASSID%",htmlEscape(String(STA_SSID))); s.replace("%STAPASS%",htmlEscape(String(STA_PASSWORD))); s.replace("%STATO%",String(staTimeoutSeconds));
  return s;
}

void redirect() {
  server.sendHeader("Location", "/");
  server.send(303);
}

// ------------------------------
// Upload handling
// ------------------------------
void handleFileUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadSucceeded = false;
    uploadMessage = "Upload failed";

    String patternName = server.arg("patternName");
    String fileName = String(upload.filename);
    String lower = fileName;
    lower.toLowerCase();

    bool isWav = lower.endsWith(".wav");
    bool isTxt = lower.endsWith(".txt");
    if (!isWav && !isTxt) {
      uploadMessage = "Only .txt and .wav chime files are supported";
      Serial.println(uploadMessage);
      return;
    }

    String base = "/welcome";
    if (patternName == "coin") base = "/coin";
    else if (patternName == "warning") base = "/warning";
    else if (patternName == "timeup") base = "/timeup";

    String outputPath = base + (isWav ? ".wav" : ".txt");
    String alternatePath = base + (isWav ? ".txt" : ".wav");

    // Ensure the newly uploaded format wins. The player checks TXT first, so
    // an old TXT must not shadow a newly uploaded WAV.
    if (LittleFS.exists(outputPath)) LittleFS.remove(outputPath);
    if (LittleFS.exists(alternatePath)) LittleFS.remove(alternatePath);

    uploadFile = LittleFS.open(outputPath, "w");
    if (!uploadFile) {
      uploadMessage = "Failed to open LittleFS upload file";
      Serial.println(uploadMessage);
    } else {
      uploadMessage = "Uploading";
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      uploadSucceeded = true;
      uploadMessage = "Upload complete";
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    uploadSucceeded = false;
    uploadMessage = "Upload aborted";
  }
}

// ------------------------------
// Setup / routes
// ------------------------------
void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS failed");
  }

  loadPersistentState();
  loadExtendedState();

  if (!pinsAreValid()) {
    revertToDefaults();
  }

  setupDisplay();

  pinMode(coinPin, coinActiveLow ? INPUT_PULLUP : INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, relayActiveLow ? HIGH : LOW);
  pinMode(relay2Pin, OUTPUT);
  digitalWrite(relay2Pin, relay2ActiveLow ? HIGH : LOW);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, buzzerActiveLow ? HIGH : LOW);
  if (deviceRole == ROLE_SLAVE) pinMode(requestPin, requestActiveLow ? INPUT_PULLUP : INPUT);

  startNetwork();

  server.on("/", []() {
    server.send(200, "text/html", page());
  });

  server.on("/relay1", []() {
    relayState = !relayState;
    setRelay1(relayState);
    redirect();
  });

  server.on("/relay2", []() {
    relay2State = !relay2State;
    setRelay2(relay2State);
    redirect();
  });

  server.on("/warn", []() {
    playWarningChime();
    redirect();
  });

  server.on("/resetTimer", []() {
    resetTimer();
    redirect();
  });

  server.on("/clearSales", []() {
    clearSales();
    redirect();
  });

  server.on("/settings", HTTP_POST, []() {
    if (!server.hasArg("save")) {
      redirect();
      return;
    }

    String newApName = server.hasArg("apName") ? server.arg("apName") : String(AP_NAME);
    String newApPassword = server.hasArg("apPassword") ? server.arg("apPassword") : String(AP_PASSWORD);
    newApName.trim();

    uint8_t newCoinPin = server.hasArg("coinPin") ? constrain(server.arg("coinPin").toInt(), 0, 16) : coinPin;
    uint8_t newRelayPin = server.hasArg("relayPin") ? constrain(server.arg("relayPin").toInt(), 0, 16) : relayPin;
    uint8_t newRelay2Pin = server.hasArg("relay2Pin") ? constrain(server.arg("relay2Pin").toInt(), 0, 16) : relay2Pin;
    uint8_t newBuzzerPin = FIXED_BUZZER_PIN;
    uint8_t newDisplayClk = server.hasArg("displayClk") ? constrain(server.arg("displayClk").toInt(), 0, 16) : displayClk;
    uint8_t newDisplayDio = server.hasArg("displayDio") ? constrain(server.arg("displayDio").toInt(), 0, 16) : displayDio;

    if (!apConfigIsValid(newApName, newApPassword)) {
      server.send(400, "text/plain", "Invalid AP settings. Name must be 1-32 characters; password must be empty or 8-63 characters.");
      return;
    }

    if (!pinSetIsUnique(newCoinPin, newRelayPin, newRelay2Pin, newBuzzerPin, newDisplayClk, newDisplayDio)) {
      server.send(400, "text/plain", "Invalid pin mapping. Pins must be unique, GPIO6-11 are reserved for flash, and values must be within GPIO0-16.");
      return;
    }

    bool pinConfigChanged = newCoinPin != coinPin || newRelayPin != relayPin ||
                            newRelay2Pin != relay2Pin || newBuzzerPin != buzzerPin ||
                            newDisplayClk != displayClk || newDisplayDio != displayDio;

    bool newCoinActiveLow = server.hasArg("coinActiveLow");
    bool newRelayActiveLow = server.hasArg("relayActiveLow");
    bool newRelay2ActiveLow = server.hasArg("relay2ActiveLow");
    bool newBuzzerActiveLow = server.hasArg("buzzerActiveLow");

    bool polarityChanged = newCoinActiveLow != coinActiveLow ||
                           newRelayActiveLow != relayActiveLow ||
                           newRelay2ActiveLow != relay2ActiveLow ||
                           newBuzzerActiveLow != buzzerActiveLow;

    newApName.toCharArray(AP_NAME, sizeof(AP_NAME));
    newApPassword.toCharArray(AP_PASSWORD, sizeof(AP_PASSWORD));

    // If hardware mapping/polarity changes, stop the old outputs safely first.
    if (pinConfigChanged || polarityChanged) {
      setRelay1(false);
      setRelay2(false);
      stopChime();
    }

    coinPin = newCoinPin;
    relayPin = newRelayPin;
    relay2Pin = newRelay2Pin;
    buzzerPin = FIXED_BUZZER_PIN;
    displayClk = newDisplayClk;
    displayDio = newDisplayDio;

    coinActiveLow = newCoinActiveLow;
    relayActiveLow = newRelayActiveLow;
    relay2ActiveLow = newRelay2ActiveLow;
    buzzerActiveLow = newBuzzerActiveLow;

    if (server.hasArg("addMinutes")) addMinutes = constrain(server.arg("addMinutes").toInt(), 0, 180);
    if (server.hasArg("addSeconds")) addSeconds = constrain(server.arg("addSeconds").toInt(), 0, 59);
    if (server.hasArg("warningSeconds")) warningSeconds = constrain(server.arg("warningSeconds").toInt(), 0, 120);
    if (server.hasArg("chimeDurationSeconds")) chimeDurationSeconds = constrain(server.arg("chimeDurationSeconds").toInt(), 1, 120);
    if (server.hasArg("speedPct")) countdownSpeedPercent = constrain(server.arg("speedPct").toInt(), 0, 50);

    if (pinConfigChanged || polarityChanged) {
      pinMode(coinPin, coinActiveLow ? INPUT_PULLUP : INPUT);
      pinMode(relayPin, OUTPUT);
      pinMode(relay2Pin, OUTPUT);
      pinMode(buzzerPin, OUTPUT);
      setupDisplay();
      setRelays(timerRunning && remainingSeconds > 0);
      setBuzzer(false);
    }

    if(server.hasArg("role")) deviceRole=(DeviceRole)constrain(server.arg("role").toInt(),0,2);
    if(server.hasArg("networkMode")) networkMode=(NetworkMode)constrain(server.arg("networkMode").toInt(),0,1);
    if(server.hasArg("staSsid")) server.arg("staSsid").toCharArray(STA_SSID,sizeof(STA_SSID));
    if(server.hasArg("staPassword")) server.arg("staPassword").toCharArray(STA_PASSWORD,sizeof(STA_PASSWORD));
    if(server.hasArg("deviceName")) server.arg("deviceName").toCharArray(DEVICE_NAME,sizeof(DEVICE_NAME));
    if(server.hasArg("nodeId")) nodeId=constrain(server.arg("nodeId").toInt(),1,65535);
    if(server.hasArg("requestPin")) requestPin=constrain(server.arg("requestPin").toInt(),0,16);
    requestActiveLow=server.hasArg("requestActiveLow");
    if(server.hasArg("staTimeout")) staTimeoutSeconds=constrain(server.arg("staTimeout").toInt(),3,120);
    if(server.hasArg("requestTimeout")) requestTimeoutSeconds=constrain(server.arg("requestTimeout").toInt(),3,300);
    if(server.hasArg("coinSessionTimeout")) coinSessionTimeoutSeconds=constrain(server.arg("coinSessionTimeout").toInt(),3,300);
    if(server.hasArg("heartbeatTimeout")) heartbeatTimeoutSeconds=constrain(server.arg("heartbeatTimeout").toInt(),3,300);
    masterAutoDiscover=server.hasArg("masterAutoDiscover");
    if(server.hasArg("masterIp")) server.arg("masterIp").toCharArray(MASTER_IP,sizeof(MASTER_IP));
    saveExtendedState();

    savePersistentState();

    // AP name/password are stored immediately but take effect after restart.
    redirect();
  });

  server.on("/upload", HTTP_POST, []() {
    server.send(uploadSucceeded ? 200 : 400, "text/plain", uploadMessage);
  }, handleFileUpload);

  server.begin();

  // restore timer state from EEPROM if present
  if (remainingSeconds > 0 && state.timerRunning) {
    timerRunning = true;
    remainingSecondsFloat = (float)remainingSeconds;
    lastCountdownMillis = millis();
    setRelays(true);
  } else {
    remainingSeconds = 0;
    remainingSecondsFloat = 0.0f;
    timerRunning = false;
  }

  showRemaining();

  // Startup chime. The web server is already running, so this is non-blocking.
  lastNearEndStartedAt = millis();
  playWelcomeChime();

  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  handleUdp();
  handleRequestButton();
  maintainCentral();

  handleCoinInput();
  updateTimerState();
  updateBuzzer();
  updateChimePlayer();
  updateRoleDisplay();

  // keep sales buckets current
  updateSalesBuckets();

}

void updateBuzzer() {
  uint32_t now = millis();

  if (buzzerUntil != 0 && (int32_t)(now - buzzerUntil) >= 0) {
    setBuzzer(false);
    buzzerUntil = 0;
  }
}