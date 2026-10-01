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