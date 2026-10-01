#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <TM1637Display.h>
#include <vector>
#include <math.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2SNoDAC.h>
#include "embedded_audio_public.h"

extern "C" {
#include <user_interface.h>
}

/*
  BLAZE Pisonet Timer - embedded audio edition v2

  Required Arduino library:
    ESP8266Audio by Earle F. Philhower

  Important hardware note:
    Full embedded audio uses the ESP8266 hardware I2S NoDAC output, which is
    fixed to GPIO3 / RX on ESP8266. Move the existing PASSIVE PIEZO connection
    from D5 to RX/GPIO3. Do not directly connect an 8-ohm speaker or headphones
    to an ESP8266 GPIO. No SD card, DFPlayer, DAC, or storage module is used.
    An active buzzer cannot reproduce the embedded recordings.

  Recommended board menu settings:
    CPU Frequency: 160 MHz
    Flash Size: 4 MB (any layout with at least ~1 MB sketch space)
*/

// ------------------------------
// EEPROM + persistent storage
// ------------------------------
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
static_assert(sizeof(PersistentState) <= 512, "PersistentState exceeds EEPROM allocation");

// ------------------------------
// Board defaults
// ------------------------------
char AP_NAME[33] = "ESP8266-Control";
char AP_PASSWORD[65] = "CHANGE_ME";

ESP8266WebServer server(80);

// Default board mapping
uint8_t coinPin = 0;      // GPIO0 / FLASH button
uint8_t relayPin = 5;     // D1
uint8_t relay2Pin = 4;    // D2
const uint8_t AUDIO_OUTPUT_PIN = 3; // RX / GPIO3, fixed ESP8266 I2S NoDAC output
uint8_t buzzerPin = AUDIO_OUTPUT_PIN;
uint8_t displayClk = 12;  // D6
uint8_t displayDio = 13;  // D7

bool coinActiveLow = true;
bool relayActiveLow = true;
bool relay2ActiveLow = false;
bool buzzerActiveLow = false; // retained in EEPROM layout; passive audio output uses normal polarity

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
uint32_t lastAcceptedCoinPulseAt = 0;

uint32_t lastCountdownMillis = 0;
float remainingSecondsFloat = 0.0f;

TM1637Display* timerDisplay = nullptr;

enum class EmbeddedAudioTrack : uint8_t {
  None,
  Welcome,
  NearEnd,
  Timeout,
  Manual
};

// MP3 decoding needs a sizeable work area. Use the exact size requested by
// the installed ESP8266Audio library and allocate it once, avoiding repeated
// heap allocation when faa.mp3 restarts every two seconds.
static uint8_t mp3CodecSpace[AudioGeneratorMP3::preAllocSize()] __attribute__((aligned(8)));
AudioFileSourcePROGMEM* embeddedAudioFile = nullptr;
AudioGeneratorMP3* embeddedAudioDecoder = nullptr;
AudioOutputI2SNoDAC* embeddedAudioOutput = nullptr;
bool embeddedAudioEngineReady = false;
EmbeddedAudioTrack embeddedAudioTrack = EmbeddedAudioTrack::None;
uint32_t lastNearEndAudioStartedAt = 0;
bool previousNearEndCondition = false;

// Non-blocking chime scheduler
std::vector<int> chimeFreqs;
std::vector<int> chimeDurs;
uint32_t chimeStepStartedAt = 0;
uint32_t chimeStepDurationMs = 0;
bool chimePlaying = false;
size_t chimeIndex = 0;

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
bool initEmbeddedAudioEngine();
void stopEmbeddedAudio();
void updateEmbeddedAudio();
void manageAudioPolicy();
void stopTonePattern();
bool startEmbeddedAudio(const uint8_t* data, uint32_t length, EmbeddedAudioTrack track);

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

uint32_t roundedRemainingSeconds(float seconds) {
  if (seconds <= 0.0f) return 0;
  // Ceil prevents the display from showing 00:00 while a fraction of a
  // second is still active.
  return (uint32_t)ceilf(seconds);
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
  buzzerPin = AUDIO_OUTPUT_PIN; // GPIO3 is fixed by ESP8266 I2S NoDAC
  displayClk = state.displayClk;
  displayDio = state.displayDio;

  coinActiveLow = state.coinActiveLow;
  relayActiveLow = state.relayActiveLow;
  relay2ActiveLow = state.relay2ActiveLow;
  buzzerActiveLow = false;

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
  strcpy(AP_NAME, "ESP8266-Control");
  strcpy(AP_PASSWORD, "CHANGE_ME");

  coinPin = 0;
  relayPin = 5;
  relay2Pin = 4;
  buzzerPin = AUDIO_OUTPUT_PIN;
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
  state.buzzerPin = AUDIO_OUTPUT_PIN;
  state.displayClk = displayClk;
  state.displayDio = displayDio;

  state.coinActiveLow = coinActiveLow;
  state.relayActiveLow = relayActiveLow;
  state.relay2ActiveLow = relay2ActiveLow;
  state.buzzerActiveLow = false;

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
  EEPROM.begin(512);
  EEPROM.get(0, state);

  if (state.magic != EEPROM_MAGIC) {
    loadDefaults();
    savePersistentState();
    return;
  }

  applyStateToRuntime();

  // Clamp stale or corrupt EEPROM values before they reach timer/audio logic.
  if (addMinutes > 180) addMinutes = 1;
  if (addSeconds > 59) addSeconds = 0;
  if (warningSeconds > 120) warningSeconds = 10;
  if (chimeDurationSeconds < 1 || chimeDurationSeconds > 120) chimeDurationSeconds = 30;
  if (countdownSpeedPercent > 50) countdownSpeedPercent = 0;
}

void savePersistentState() {
  state.magic = EEPROM_MAGIC;
  strcpy(state.apName, AP_NAME);
  strcpy(state.apPassword, AP_PASSWORD);

  state.coinPin = coinPin;
  state.relayPin = relayPin;
  state.relay2Pin = relay2Pin;
  state.buzzerPin = AUDIO_OUTPUT_PIN;
  state.displayClk = displayClk;
  state.displayDio = displayDio;

  state.coinActiveLow = coinActiveLow;
  state.relayActiveLow = relayActiveLow;
  state.relay2ActiveLow = relay2ActiveLow;
  state.buzzerActiveLow = false;

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
  buzzerPin = AUDIO_OUTPUT_PIN;
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
  if (!timerDisplay) {
    Serial.println("TM1637 allocation failed");
    return;
  }
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

// ------------------------------
// Relays + audio output
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
  digitalWrite(buzzerPin, on ? HIGH : LOW);
}

void noBuzzer() {
  noTone(buzzerPin);
  setBuzzer(false);
}

String embeddedAudioTrackName() {
  switch (embeddedAudioTrack) {
    case EmbeddedAudioTrack::Welcome: return "Welcome audio";
    case EmbeddedAudioTrack::NearEnd: return "Near-end warning";
    case EmbeddedAudioTrack::Timeout: return "Time-over audio";
    case EmbeddedAudioTrack::Manual: return "Manual audio test";
    default: return "Idle";
  }
}

bool initEmbeddedAudioEngine() {
  if (embeddedAudioEngineReady) return true;

  embeddedAudioFile = new AudioFileSourcePROGMEM();
  embeddedAudioOutput = new AudioOutputI2SNoDAC();
  embeddedAudioDecoder = new AudioGeneratorMP3(mp3CodecSpace, sizeof(mp3CodecSpace));

  if (!embeddedAudioFile || !embeddedAudioOutput || !embeddedAudioDecoder) {
    delete embeddedAudioDecoder;
    delete embeddedAudioOutput;
    delete embeddedAudioFile;
    embeddedAudioDecoder = nullptr;
    embeddedAudioOutput = nullptr;
    embeddedAudioFile = nullptr;
    embeddedAudioEngineReady = false;
    return false;
  }

  embeddedAudioOutput->SetGain(0.55f);
  embeddedAudioOutput->SetOutputModeMono(true);
  embeddedAudioOutput->SetOversampling(32);
  embeddedAudioEngineReady = true;
  return true;
}

void stopEmbeddedAudio() {
  if (embeddedAudioDecoder && embeddedAudioDecoder->isRunning()) {
    embeddedAudioDecoder->stop();
  } else {
    if (embeddedAudioFile) embeddedAudioFile->close();
    if (embeddedAudioOutput) embeddedAudioOutput->stop();
  }

  embeddedAudioTrack = EmbeddedAudioTrack::None;
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
}

void stopTonePattern() {
  chimePlaying = false;
  chimeIndex = 0;
  chimeStepDurationMs = 0;
  chimeFreqs.clear();
  chimeDurs.clear();
  noBuzzer();
}

bool startEmbeddedAudio(const uint8_t* data, uint32_t length, EmbeddedAudioTrack track) {
  if (!data || length == 0) return false;
  if (!initEmbeddedAudioEngine()) return false;

  // Stop I2S playback before touching the tone generator on the shared pin.
  stopEmbeddedAudio();
  stopTonePattern();

  if (!embeddedAudioFile->open(data, length)) return false;
  if (!embeddedAudioDecoder->begin(embeddedAudioFile, embeddedAudioOutput)) {
    embeddedAudioDecoder->stop();
    embeddedAudioFile->close();
    embeddedAudioOutput->stop();
    embeddedAudioTrack = EmbeddedAudioTrack::None;
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
    return false;
  }

  embeddedAudioTrack = track;
  if (track == EmbeddedAudioTrack::NearEnd) {
    lastNearEndAudioStartedAt = millis();
  }
  return true;
}

bool playWelcomeAudio(bool manual = false) {
  return startEmbeddedAudio(WELCOME_AUDIO, WELCOME_AUDIO_LEN,
                            manual ? EmbeddedAudioTrack::Manual : EmbeddedAudioTrack::Welcome);
}

bool playNearEndAudio(bool manual = false) {
  return startEmbeddedAudio(NEAR_END_AUDIO, NEAR_END_AUDIO_LEN,
                            manual ? EmbeddedAudioTrack::Manual : EmbeddedAudioTrack::NearEnd);
}

bool playTimeoutAudio(bool manual = false) {
  return startEmbeddedAudio(TIMEOUT_AUDIO, TIMEOUT_AUDIO_LEN,
                            manual ? EmbeddedAudioTrack::Manual : EmbeddedAudioTrack::Timeout);
}

void updateEmbeddedAudio() {
  if (!embeddedAudioDecoder) return;

  if (embeddedAudioDecoder->isRunning()) {
    if (!embeddedAudioDecoder->loop()) stopEmbeddedAudio();
  } else if (embeddedAudioTrack != EmbeddedAudioTrack::None) {
    stopEmbeddedAudio();
  }
}

// ------------------------------
// Coin tone scheduler
// ------------------------------
void queuePattern(const std::vector<int>& freqs, const std::vector<int>& durs) {
  if (freqs.empty() || durs.empty() || freqs.size() != durs.size()) {
    stopTonePattern();
    return;
  }

  stopEmbeddedAudio();
  stopTonePattern();

  chimeFreqs.clear();
  chimeDurs.clear();

  uint32_t maxDurationMs = (uint32_t)chimeDurationSeconds * 1000UL;
  uint32_t totalMs = 0;

  chimeFreqs.reserve(freqs.size());
  chimeDurs.reserve(durs.size());

  for (size_t i = 0; i < freqs.size(); i++) {
    if (totalMs >= maxDurationMs) break;

    uint32_t dur = (uint32_t)durs[i];
    if (totalMs + dur > maxDurationMs) dur = maxDurationMs - totalMs;
    if (dur == 0) break;

    chimeFreqs.push_back(freqs[i]);
    chimeDurs.push_back((int)dur);
    totalMs += dur;
  }

  if (chimeFreqs.empty()) {
    stopTonePattern();
    return;
  }

  chimeIndex = 0;
  chimePlaying = true;
  chimeStepStartedAt = millis();
  chimeStepDurationMs = (uint32_t)chimeDurs[0];
  tone(buzzerPin, chimeFreqs[0], chimeStepDurationMs + 25);
}

void updateChimePlayer() {
  if (!chimePlaying) return;

  uint32_t now = millis();
  if ((uint32_t)(now - chimeStepStartedAt) < chimeStepDurationMs) return;

  chimeIndex++;
  if (chimeIndex >= chimeFreqs.size()) {
    stopTonePattern();
    return;
  }

  chimeStepStartedAt = now;
  chimeStepDurationMs = (uint32_t)chimeDurs[chimeIndex];
  tone(buzzerPin, chimeFreqs[chimeIndex], chimeStepDurationMs + 25);
}

// Insert-coin chime remains the original four-note pattern.
void playCoinChime() {
  std::vector<int> freqs;
  std::vector<int> durs;

  const int notes[] = { 880, 1180, 880, 1180 };
  const int dur[] = { 110, 150, 110, 180 };

  freqs.reserve(4);
  durs.reserve(4);
  for (int i = 0; i < 4; i++) {
    freqs.push_back(notes[i]);
    durs.push_back(dur[i]);
  }

  queuePattern(freqs, durs);
}

void manageAudioPolicy() {
  uint32_t now = millis();
  bool nearEnd = timerRunning && warningSeconds > 0 && remainingSeconds > 0 &&
                 remainingSeconds <= warningSeconds;

  if (nearEnd && !previousNearEndCondition) {
    // Play immediately when the countdown first enters the warning range.
    lastNearEndAudioStartedAt = now - 2000UL;
  }

  if (!timerRunning) {
    if (embeddedAudioTrack == EmbeddedAudioTrack::NearEnd) stopEmbeddedAudio();
    previousNearEndCondition = false;
    return;
  }

  // Any paid/running session cancels startup or time-over playback.
  if (embeddedAudioTrack == EmbeddedAudioTrack::Welcome ||
      embeddedAudioTrack == EmbeddedAudioTrack::Timeout) {
    stopEmbeddedAudio();
  }

  if (!nearEnd) {
    if (embeddedAudioTrack == EmbeddedAudioTrack::NearEnd) stopEmbeddedAudio();
  } else if (embeddedAudioTrack == EmbeddedAudioTrack::None && !chimePlaying &&
             (uint32_t)(now - lastNearEndAudioStartedAt) >= 2000UL) {
    playNearEndAudio(false);
  }

  previousNearEndCondition = nearEnd;
}

// ------------------------------
// Timer logic + countdown speed percent
// ------------------------------
float getCountdownScale() {
  return 1.0f + ((float)countdownSpeedPercent / 100.0f);
}

void startTimerFromSeconds(uint32_t secs) {
  stopEmbeddedAudio();
  stopTonePattern();

  if (secs == 0) {
    remainingSeconds = 0;
    timerRunning = false;
    remainingSecondsFloat = 0.0f;
    previousNearEndCondition = false;
    setRelays(false);
    showRemaining();
    savePersistentState();
    return;
  }

  remainingSecondsFloat = (float)secs;
  remainingSeconds = secs;
  timerRunning = true;
  previousNearEndCondition = false;
  lastNearEndAudioStartedAt = millis();
  lastCountdownMillis = millis();
  setRelays(true);
  showRemaining();
  savePersistentState();
}

void addCoinTime() {
  uint32_t now = millis();
  uint32_t addSecondsFromCoin = ((uint32_t)addMinutes * 60UL) + (uint32_t)addSeconds;

  // Coin input always cancels welcome, near-end, or timeout audio first.
  stopEmbeddedAudio();
  stopTonePattern();

  if (addSecondsFromCoin > 0) {
    if (timerRunning) {
      remainingSecondsFloat += (float)addSecondsFromCoin;
    } else {
      remainingSecondsFloat = (float)addSecondsFromCoin;
      timerRunning = true;
    }

    remainingSeconds = roundedRemainingSeconds(remainingSecondsFloat);
    lastCountdownMillis = now;
    setRelays(true);
  }

  // Keep FAA from immediately replacing the insert-coin chime if the newly
  // added amount still leaves the timer inside the warning range. Mark the
  // current warning state as already entered and wait a full two seconds.
  lastNearEndAudioStartedAt = now;
  previousNearEndCondition = timerRunning && warningSeconds > 0 &&
                             remainingSeconds > 0 &&
                             remainingSeconds <= warningSeconds;

  showRemaining();
  playCoinChime();
  savePersistentState();
}

void updateTimerState() {
  uint32_t now = millis();
  uint32_t previousWholeSeconds = remainingSeconds;

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
    stopEmbeddedAudio();
    stopTonePattern();
    playTimeoutAudio(false);
    savePersistentState();
    return;
  }

  remainingSeconds = roundedRemainingSeconds(remainingSecondsFloat);
  if (remainingSeconds != previousWholeSeconds) showRemaining();
}

// reset timer to zero via button/web route
void resetTimer() {
  stopEmbeddedAudio();
  stopTonePattern();
  remainingSeconds = 0;
  remainingSecondsFloat = 0.0f;
  timerRunning = false;
  previousNearEndCondition = false;
  lastNearEndAudioStartedAt = 0;
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
  bool coinTriggered = digitalRead(coinPin) == (coinActiveLow ? LOW : HIGH);
  uint32_t now = millis();

  if (coinTriggered && !coinLatch) {
    coinLatch = true;

    // Reject very fast contact bounce while still allowing normal multi-pulse
    // electronic coin acceptors.
    if (lastAcceptedCoinPulseAt == 0 || (uint32_t)(now - lastAcceptedCoinPulseAt) >= 50UL) {
      lastAcceptedCoinPulseAt = now;
      addCoinTime();
      incrementSales();
    }
  } else if (!coinTriggered) {
    coinLatch = false;
  }
}

// ------------------------------
// Web page
// ------------------------------
String page() {
  String s;
  s.reserve(15000);

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
  <button class="tab-btn" onclick="showTab('audio',this)">Audio</button>
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
          <small>Main computer power · GPIO)rawliteral");

  s += String(relayPin);

  s += F(R"rawliteral(</small>
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
          <small>Secondary output · GPIO)rawliteral");

  s += String(relay2Pin);

  s += F(R"rawliteral(</small>
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
          Test Near-End Audio
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
        <label>Tone Pattern Maximum Duration</label>
        <input name="chimeDurationSeconds"
               type="number"
               min="1"
               max="120"
               value=")rawliteral");

  s += String(chimeDurationSeconds);

  s += F(R"rawliteral(">
        <div class="help">Applies to generated beep patterns only; embedded MP3 lengths are fixed.</div>
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
        <label>Embedded Audio Output</label>
        <input type="text" value="GPIO3 / RX (fixed)" disabled>
        <div class="help">ESP8266 I2S NoDAC audio is fixed to RX/GPIO3.</div>
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

<section id="audio" class="tab-page">

<div class="card">
  <div class="card-title">
    <div>
      <h2>Embedded Audio Chimes</h2>
      <p>The three supplied recordings are stored directly in program flash.</p>
    </div>
  </div>

  <div class="grid3">
    <div class="stat">
      <span>Welcome</span>
      <strong style="font-size:18px">tacos.mp3</strong>
      <p>First 40 seconds; plays once at idle startup.</p>
      <a class="btn btn-secondary" href="/testWelcome">Test Welcome</a>
    </div>

    <div class="stat">
      <span>Near End</span>
      <strong style="font-size:18px">faa.mp3</strong>
      <p>Repeats about every 2 seconds while inside the warning range.</p>
      <a class="btn btn-warning" href="/testNearEnd">Test Warning</a>
    </div>

    <div class="stat">
      <span>Time Over</span>
      <strong style="font-size:18px">timeout.mp3</strong>
      <p>Plays completely unless a coin starts or extends the timer.</p>
      <a class="btn btn-danger" href="/testTimeout">Test Time Over</a>
    </div>
  </div>

  <br>
  <p>Current audio state: <b>)rawliteral");

  s += embeddedAudioTrackName();

  s += F(R"rawliteral(</b></p>
  <a class="btn btn-secondary" href="/stopAudio">Stop Audio</a>

  <div class="group-title">Storage and output</div>
  <p>The files were converted to mono 16 kHz, 16 kbps MP3 and embedded in flash. The welcome recording is trimmed to 40 seconds. No SD card, DFPlayer, DAC module, or internet connection is used.</p>
  <p>Connect the existing <b>passive piezo</b> to <span class="code">RX / GPIO3</span> and GND. Do not directly connect an 8-ohm speaker or headphones to the GPIO. An active buzzer can only beep and cannot reproduce these recordings.</p>
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
      <td>Embedded Audio Output</td>
      <td>RX / GPIO3 (fixed)</td>
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
      <td>Embedded Audio</td>
      <td>16 kHz mono / 16 kbps MP3 in program flash</td>
    </tr>

    <tr>
      <td>Encoded Audio Size</td>
      <td>)rawliteral");

  s += String(EMBEDDED_AUDIO_TOTAL_BYTES / 1024UL);

  s += F(R"rawliteral( KB</td>
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

</body>
</html>
)rawliteral");

  return s;
}

void redirect() {
  server.sendHeader("Location", "/");
  server.send(303);
}

// ------------------------------
// Setup / routes
// ------------------------------
void setup() {
  Serial.begin(115200);
  system_update_cpu_freq(SYS_CPU_160MHZ);

  loadPersistentState();
  buzzerPin = AUDIO_OUTPUT_PIN;
  buzzerActiveLow = false;

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
  digitalWrite(buzzerPin, LOW);

  WiFi.mode(WIFI_AP);
  bool apStarted = false;
  if (apConfigIsValid(String(AP_NAME), String(AP_PASSWORD))) {
    apStarted = (strlen(AP_PASSWORD) == 0) ? WiFi.softAP(AP_NAME) : WiFi.softAP(AP_NAME, AP_PASSWORD);
  }

  if (!apStarted) {
    Serial.println("Invalid/stale AP config; using safe defaults");
    strcpy(AP_NAME, "ESP8266-Control");
    strcpy(AP_PASSWORD, "CHANGE_ME");
    savePersistentState();
    WiFi.softAP(AP_NAME, AP_PASSWORD);
  }

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.send(200, "text/html; charset=utf-8", page());
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
    playNearEndAudio(true);
    redirect();
  });

  server.on("/testWelcome", []() {
    playWelcomeAudio(true);
    redirect();
  });

  server.on("/testNearEnd", []() {
    playNearEndAudio(true);
    redirect();
  });

  server.on("/testTimeout", []() {
    playTimeoutAudio(true);
    redirect();
  });

  server.on("/stopAudio", []() {
    stopEmbeddedAudio();
    stopTonePattern();
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
    uint8_t newDisplayClk = server.hasArg("displayClk") ? constrain(server.arg("displayClk").toInt(), 0, 16) : displayClk;
    uint8_t newDisplayDio = server.hasArg("displayDio") ? constrain(server.arg("displayDio").toInt(), 0, 16) : displayDio;

    if (!apConfigIsValid(newApName, newApPassword)) {
      server.send(400, "text/plain", "Invalid AP settings. Name must be 1-32 characters; password must be empty or 8-63 characters.");
      return;
    }

    if (!pinSetIsUnique(newCoinPin, newRelayPin, newRelay2Pin, AUDIO_OUTPUT_PIN, newDisplayClk, newDisplayDio)) {
      server.send(400, "text/plain", "Invalid pin mapping. Pins must be unique, GPIO3 is reserved for embedded audio, GPIO6-11 are reserved for flash, and values must be within GPIO0-16.");
      return;
    }

    bool pinConfigChanged = newCoinPin != coinPin || newRelayPin != relayPin ||
                            newRelay2Pin != relay2Pin ||
                            newDisplayClk != displayClk || newDisplayDio != displayDio;

    bool newCoinActiveLow = server.hasArg("coinActiveLow");
    bool newRelayActiveLow = server.hasArg("relayActiveLow");
    bool newRelay2ActiveLow = server.hasArg("relay2ActiveLow");

    bool polarityChanged = newCoinActiveLow != coinActiveLow ||
                           newRelayActiveLow != relayActiveLow ||
                           newRelay2ActiveLow != relay2ActiveLow;

    newApName.toCharArray(AP_NAME, sizeof(AP_NAME));
    newApPassword.toCharArray(AP_PASSWORD, sizeof(AP_PASSWORD));

    // If hardware mapping/polarity changes, stop the old outputs safely first.
    if (pinConfigChanged || polarityChanged) {
      setRelay1(false);
      setRelay2(false);
      stopEmbeddedAudio();
      stopTonePattern();
    }

    coinPin = newCoinPin;
    relayPin = newRelayPin;
    relay2Pin = newRelay2Pin;
    buzzerPin = AUDIO_OUTPUT_PIN;
    displayClk = newDisplayClk;
    displayDio = newDisplayDio;

    coinActiveLow = newCoinActiveLow;
    relayActiveLow = newRelayActiveLow;
    relay2ActiveLow = newRelay2ActiveLow;
    buzzerActiveLow = false;

    if (server.hasArg("addMinutes")) addMinutes = constrain(server.arg("addMinutes").toInt(), 0, 180);
    if (server.hasArg("addSeconds")) addSeconds = constrain(server.arg("addSeconds").toInt(), 0, 59);
    if (server.hasArg("warningSeconds")) warningSeconds = constrain(server.arg("warningSeconds").toInt(), 0, 120);
    if (server.hasArg("chimeDurationSeconds")) chimeDurationSeconds = constrain(server.arg("chimeDurationSeconds").toInt(), 1, 120);
    if (server.hasArg("speedPct")) countdownSpeedPercent = constrain(server.arg("speedPct").toInt(), 0, 50);

    if (pinConfigChanged || polarityChanged) {
      coinLatch = false;
      lastAcceptedCoinPulseAt = 0;
      pinMode(coinPin, coinActiveLow ? INPUT_PULLUP : INPUT);
      pinMode(relayPin, OUTPUT);
      pinMode(relay2Pin, OUTPUT);
      pinMode(buzzerPin, OUTPUT);
      setupDisplay();
      setRelays(timerRunning && remainingSeconds > 0);
      setBuzzer(false);
    }

    savePersistentState();

    // AP name/password are stored immediately but take effect after restart.
    redirect();
  });


  server.begin();

  // Restore timer state from EEPROM if present. Without an RTC, powered-off
  // time cannot be subtracted, so the last persisted paid balance is restored.
  bool restoredStateNormalized = false;
  if (remainingSeconds > 0 && state.timerRunning) {
    timerRunning = true;
    remainingSecondsFloat = (float)remainingSeconds;
    lastCountdownMillis = millis();
    setRelays(true);
  } else {
    restoredStateNormalized = remainingSeconds != 0 || timerRunning || state.timerRunning;
    remainingSeconds = 0;
    remainingSecondsFloat = 0.0f;
    timerRunning = false;
    setRelays(false);
  }

  if (restoredStateNormalized) savePersistentState();
  showRemaining();

  if (!initEmbeddedAudioEngine()) {
    Serial.println("Embedded audio engine allocation failed");
  } else if (!timerRunning) {
    // Play only the first 40 seconds of tacos.mp3 at idle startup.
    playWelcomeAudio(false);
  }

  Serial.print("Embedded audio bytes: ");
  Serial.println(EMBEDDED_AUDIO_TOTAL_BYTES);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Web server started");
}

void loop() {
  // Feed the MP3 decoder before and after web handling to reduce audio gaps.
  updateEmbeddedAudio();
  server.handleClient();
  updateEmbeddedAudio();

  handleCoinInput();
  updateTimerState();
  updateChimePlayer();
  manageAudioPolicy();

  updateSalesBuckets();

  // Timer state is persisted on coin pulses, settings changes, reset, and
  // expiration. Avoid periodic EEPROM commits that would wear the flash.
  yield();
}