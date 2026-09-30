/*
  BlazeTube ESP8266 Shared YouTube Terminal + Wi-Fi Repeater
  ----------------------------------------------------------
  Target core: ESP8266 Arduino Core 3.1.2
  Board: NodeMCU 1.0 / Wemos D1 mini / generic ESP-12E/ESP-12F class boards
  External Arduino libraries: NONE

  IMPORTANT Arduino IDE setting:
    Tools -> lwIP Variant -> "v2 Lower Memory"
    or "v2 Higher Bandwidth"
  Do NOT select a "(no features)" lwIP variant because NAPT/NAT needs LWIP_FEATURES.

  Recommended:
    Flash: 4 MB board, at least 1 MB filesystem
    CPU: 160 MHz for best repeater performance
    Flash mode: DIO is the safest generic choice

  First-boot access:
    AP SSID:      BlazeTube-XXXXXX  (XXXXXX = chip ID)
    AP password:  generated uniquely on first boot
    Web UI:       http://192.168.77.1/
    Admin user:   admin
    Admin pass:   generated uniquely on first boot
    Generated credentials are saved to LittleFS and printed to Serial at 115200.

  What this firmware does:
    - ESP8266 AP + STA at the same time.
    - NAPT/NAT routes AP clients through the upstream STA connection.
    - Up to 3 shared YouTube tabs.
    - All connected browsers poll one authoritative ESP session.
    - Search/tab/video/play/pause/seek changes made on one device appear on the others.
    - Session is checkpointed to LittleFS for reboot recovery.
    - YouTube browsing uses YouTube Data API v3; playback uses the official iframe player.
    - Search/home results are fetched once and cached by the ESP for all connected clients.

  What it deliberately does NOT do:
    - It does not proxy or render m.youtube.com inside the ESP8266.
    - It does not bypass YouTube login, ads, policy, CORS, CSP, or browser autoplay rules.
    - A second phone may need one user tap before its browser permits audible autoplay.

  YouTube API:
    Create a YouTube Data API v3 key in Google Cloud and enter it in Settings.
    Restrict that key to YouTube Data API v3 where practical.

  License for this sketch: MIT
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <stddef.h>

#if LWIP_FEATURES && !LWIP_IPV6
  #include <lwip/napt.h>
  #define BLAZETUBE_HAS_NAPT 1
#else
  #define BLAZETUBE_HAS_NAPT 0
#endif

// ----------------------------- Build constants -----------------------------

static const char FW_NAME[] = "BlazeTube";
static const char FW_VERSION[] = "1.1.0";
static const uint32_t CONFIG_MAGIC  = 0x42544346UL; // BTCF
static const uint32_t SESSION_MAGIC = 0x42545353UL; // BTSS
static const uint16_t CONFIG_VERSION  = 1;
static const uint16_t SESSION_VERSION = 1;

static const uint8_t TAB_COUNT = 3;
static const uint32_t SESSION_SAVE_MIN_MS = 60000UL;
static const uint32_t STA_RETRY_MS = 30000UL;
static const uint16_t NAPT_ENTRIES = 256;
static const uint8_t NAPT_PORTMAPS = 8;

static const IPAddress AP_IP(192, 168, 77, 1);
static const IPAddress AP_GW(192, 168, 77, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);
static const IPAddress FALLBACK_DNS(1, 1, 1, 1);

static const char CONFIG_FILE[] = "/config.bin";
static const char CONFIG_TMP[] = "/config.tmp";
static const char SESSION_FILE[] = "/session.bin";
static const char SESSION_TMP[] = "/session.tmp";

// ----------------------------- Persistent data -----------------------------

struct __attribute__((packed)) ConfigData {
  uint32_t magic;
  uint16_t version;
  char staSsid[33];
  char staPass[65];
  char apSsid[33];
  char apPass[65];
  char hostname[25];
  char ytApiKey[65];
  char adminPass[33];
  char region[3];
  uint8_t maxResults;
  uint8_t reserved[15];
  uint32_t crc;
};

struct __attribute__((packed)) TabState {
  char videoId[16];
  char title[97];
  char query[65];
  float basePosition;
  uint32_t baseAtMs;
  uint8_t playing;
  uint8_t reserved[3];
};

struct __attribute__((packed)) SessionData {
  uint32_t magic;
  uint16_t version;
  uint32_t revision;
  uint8_t activeTab;
  uint8_t reserved[9];
  TabState tabs[TAB_COUNT];
  uint32_t crc;
};

struct SearchResultItem {
  char videoId[16];
  char title[97];
  char channel[65];
};

struct SearchCache {
  uint8_t state;       // 0=empty, 1=fetching lease, 2=ready
  uint8_t kind;        // 1=home/popular, 2=search
  uint8_t count;
  uint8_t reserved;
  uint32_t leaseUntilMs;
  char leaseClient[17];
  char query[65];
  char error[97];
  SearchResultItem items[12];
};

ConfigData cfg;
SessionData sess;
SearchCache resultCache[TAB_COUNT];

ESP8266WebServer server(80);

bool fsReady = false;
bool naptReady = false;
bool mdnsReady = false;
bool sessionDirty = false;
uint32_t lastSessionSaveMs = 0;
uint32_t lastStaRetryMs = 0;
wl_status_t lastStaStatus = WL_IDLE_STATUS;

// ----------------------------- Utility helpers -----------------------------

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return ~crc;
}

template <size_t N>
void copyText(char (&dst)[N], const String& src) {
  size_t n = src.length();
  if (n >= N) n = N - 1;
  memcpy(dst, src.c_str(), n);
  dst[n] = '\0';
}

String jsonEscape(const char* input) {
  String out;
  if (!input) return out;
  out.reserve(strlen(input) + 12);
  for (const uint8_t* p = reinterpret_cast<const uint8_t*>(input); *p; ++p) {
    char c = static_cast<char>(*p);
    switch (c) {
      case '"':  out += F("\\\""); break;
      case '\\': out += F("\\\\"); break;
      case '\b': out += F("\\b");  break;
      case '\f': out += F("\\f");  break;
      case '\n': out += F("\\n");  break;
      case '\r': out += F("\\r");  break;
      case '\t': out += F("\\t");  break;
      default:
        if (*p < 0x20) {
          char b[7];
          snprintf(b, sizeof(b), "\\u%04x", *p);
          out += b;
        } else {
          out += c;
        }
    }
  }
  return out;
}

String htmlEscape(const char* input) {
  String out;
  if (!input) return out;
  out.reserve(strlen(input) + 16);
  for (const char* p = input; *p; ++p) {
    switch (*p) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += *p;
    }
  }
  return out;
}

bool validVideoId(const String& id) {
  if (id.length() < 6 || id.length() > 15) return false;
  for (size_t i = 0; i < id.length(); i++) {
    char c = id[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
  }
  return true;
}

bool validHostname(const String& s) {
  if (s.length() < 1 || s.length() > 24) return false;
  if (s[0] == '-' || s[s.length() - 1] == '-') return false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-') return false;
  }
  return true;
}

bool validRegion(String s) {
  s.trim();
  if (s.length() != 2) return false;
  return isalpha(static_cast<unsigned char>(s[0])) &&
         isalpha(static_cast<unsigned char>(s[1]));
}

void setDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CONFIG_MAGIC;
  cfg.version = CONFIG_VERSION;

  char apName[33];
  snprintf(apName, sizeof(apName), "BlazeTube-%06X", ESP.getChipId());
  copyText(cfg.apSsid, String(apName));
  // Generate unique first-boot credentials and persist them with the initial config.
  char apPass[25];
  char adminPass[25];
  snprintf(apPass, sizeof(apPass), "BT-%08lX-%04lX",
           static_cast<unsigned long>(ESP.random()),
           static_cast<unsigned long>(ESP.random() & 0xFFFFUL));
  snprintf(adminPass, sizeof(adminPass), "BTA-%08lX-%04lX",
           static_cast<unsigned long>(ESP.random()),
           static_cast<unsigned long>(ESP.random() & 0xFFFFUL));
  copyText(cfg.apPass, String(apPass));
  copyText(cfg.hostname, String("blazetube-") + String(ESP.getChipId(), HEX));
  copyText(cfg.adminPass, String(adminPass));
  copyText(cfg.region, F("PH"));
  cfg.maxResults = 8;

  memset(&sess, 0, sizeof(sess));
  sess.magic = SESSION_MAGIC;
  sess.version = SESSION_VERSION;
  sess.revision = 1;
  sess.activeTab = 0;
}

bool atomicWrite(const char* tmpPath, const char* finalPath, const uint8_t* data, size_t len) {
  if (!fsReady) return false;
  File f = LittleFS.open(tmpPath, "w");
  if (!f) return false;
  size_t written = f.write(data, len);
  f.flush();
  f.close();
  if (written != len) {
    LittleFS.remove(tmpPath);
    return false;
  }
  LittleFS.remove(finalPath);
  if (!LittleFS.rename(tmpPath, finalPath)) {
    LittleFS.remove(tmpPath);
    return false;
  }
  return true;
}

bool loadConfig() {
  if (!fsReady || !LittleFS.exists(CONFIG_FILE)) return false;
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f || f.size() != sizeof(ConfigData)) {
    if (f) f.close();
    return false;
  }
  ConfigData temp;
  if (f.read(reinterpret_cast<uint8_t*>(&temp), sizeof(temp)) != sizeof(temp)) {
    f.close();
    return false;
  }
  f.close();

  if (temp.magic != CONFIG_MAGIC || temp.version != CONFIG_VERSION) return false;
  uint32_t wanted = crc32Bytes(reinterpret_cast<const uint8_t*>(&temp), offsetof(ConfigData, crc));
  if (wanted != temp.crc) return false;
  cfg = temp;
  return true;
}

bool saveConfig() {
  cfg.magic = CONFIG_MAGIC;
  cfg.version = CONFIG_VERSION;
  cfg.crc = crc32Bytes(reinterpret_cast<const uint8_t*>(&cfg), offsetof(ConfigData, crc));
  return atomicWrite(CONFIG_TMP, CONFIG_FILE,
                     reinterpret_cast<const uint8_t*>(&cfg), sizeof(cfg));
}

float effectivePosition(const TabState& t, uint32_t nowMs) {
  float p = t.basePosition;
  if (t.playing) {
    p += static_cast<float>(nowMs - t.baseAtMs) / 1000.0f;
  }
  if (p < 0.0f) p = 0.0f;
  if (p > 86400.0f) p = 86400.0f;
  return p;
}

void normalizeTab(TabState& t, uint32_t nowMs) {
  t.basePosition = effectivePosition(t, nowMs);
  t.baseAtMs = nowMs;
}

bool loadSession() {
  if (!fsReady || !LittleFS.exists(SESSION_FILE)) return false;
  File f = LittleFS.open(SESSION_FILE, "r");
  if (!f || f.size() != sizeof(SessionData)) {
    if (f) f.close();
    return false;
  }
  SessionData temp;
  if (f.read(reinterpret_cast<uint8_t*>(&temp), sizeof(temp)) != sizeof(temp)) {
    f.close();
    return false;
  }
  f.close();

  if (temp.magic != SESSION_MAGIC || temp.version != SESSION_VERSION) return false;
  uint32_t wanted = crc32Bytes(reinterpret_cast<const uint8_t*>(&temp), offsetof(SessionData, crc));
  if (wanted != temp.crc) return false;

  sess = temp;
  if (sess.activeTab >= TAB_COUNT) sess.activeTab = 0;

  // millis() is a new timebase after reboot; restore at last checkpoint and pause.
  uint32_t now = millis();
  for (uint8_t i = 0; i < TAB_COUNT; i++) {
    sess.tabs[i].playing = 0;
    sess.tabs[i].baseAtMs = now;
    if (!validVideoId(String(sess.tabs[i].videoId))) {
      memset(sess.tabs[i].videoId, 0, sizeof(sess.tabs[i].videoId));
      sess.tabs[i].basePosition = 0;
    }
  }
  sess.revision++;
  return true;
}

bool saveSession() {
  if (!fsReady) return false;
  uint32_t now = millis();

  // Write a snapshot with normalized positions without disturbing live timing.
  SessionData snapshot = sess;
  for (uint8_t i = 0; i < TAB_COUNT; i++) {
    snapshot.tabs[i].basePosition = effectivePosition(sess.tabs[i], now);
    snapshot.tabs[i].baseAtMs = now;
  }
  snapshot.magic = SESSION_MAGIC;
  snapshot.version = SESSION_VERSION;
  snapshot.crc = crc32Bytes(reinterpret_cast<const uint8_t*>(&snapshot), offsetof(SessionData, crc));

  bool ok = atomicWrite(SESSION_TMP, SESSION_FILE,
                        reinterpret_cast<const uint8_t*>(&snapshot), sizeof(snapshot));
  if (ok) {
    sessionDirty = false;
    lastSessionSaveMs = now;
  }
  return ok;
}

void markSessionChanged() {
  sess.revision++;
  if (sess.revision == 0) sess.revision = 1;
  sessionDirty = true;
}

void invalidateResultCache(uint8_t tab) {
  if (tab >= TAB_COUNT) return;
  memset(&resultCache[tab], 0, sizeof(SearchCache));
}

void ensureResultCacheIdentity(uint8_t tab) {
  if (tab >= TAB_COUNT) return;
  SearchCache& c = resultCache[tab];
  const char* q = sess.tabs[tab].query;
  uint8_t wantedKind = (q && q[0]) ? 2 : 1;

  bool matches = (c.kind == wantedKind);
  if (matches && wantedKind == 2) matches = (strncmp(c.query, q, sizeof(c.query)) == 0);
  if (matches && wantedKind == 1) matches = (c.query[0] == '\0');

  if (!matches) {
    invalidateResultCache(tab);
    c.kind = wantedKind;
    if (wantedKind == 2) copyText(c.query, String(q));
  }
}

bool resultLeaseExpired(const SearchCache& c, uint32_t now) {
  if (c.state != 1) return true;
  return static_cast<int32_t>(now - c.leaseUntilMs) >= 0;
}

bool requireAdmin() {
  if (server.authenticate("admin", cfg.adminPass)) return true;
  server.requestAuthentication();
  return false;
}

void noCache() {
  server.sendHeader(F("Cache-Control"), F("no-store, no-cache, must-revalidate, max-age=0"));
  server.sendHeader(F("Pragma"), F("no-cache"));
}

// ----------------------------- Wi-Fi / NAPT -----------------------------

void configureApDns() {
  auto& dhcp = WiFi.softAPDhcpServer();
  IPAddress dns = FALLBACK_DNS;
  IPAddress upstreamDns = WiFi.dnsIP(0);
  if (WiFi.status() == WL_CONNECTED &&
      (upstreamDns[0] || upstreamDns[1] || upstreamDns[2] || upstreamDns[3])) {
    dns = upstreamDns;
  }
  dhcp.setDns(dns);
}

void startSoftAp() {
  WiFi.softAPdisconnect(true);
  delay(20);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  configureApDns();

  bool ok = WiFi.softAP(cfg.apSsid, cfg.apPass, 1, false, 4);
  Serial.printf("[AP] %s | %s | IP %s\n",
                ok ? "started" : "FAILED",
                cfg.apSsid,
                WiFi.softAPIP().toString().c_str());
}

void startNapt() {
#if BLAZETUBE_HAS_NAPT
  err_t ret = ip_napt_init(NAPT_ENTRIES, NAPT_PORTMAPS);
  if (ret == ERR_OK) {
    ret = ip_napt_enable_no(SOFTAP_IF, 1);
  }
  naptReady = (ret == ERR_OK);
  Serial.printf("[NAPT] %s (%d entries)\n", naptReady ? "enabled" : "FAILED", NAPT_ENTRIES);
#else
  naptReady = false;
  Serial.println(F("[NAPT] unavailable: choose a lwIP variant WITH features and without IPv6."));
#endif
}

void startStation() {
  if (strlen(cfg.staSsid) == 0) {
    Serial.println(F("[STA] no upstream SSID configured."));
    return;
  }
  WiFi.hostname(cfg.hostname);
  WiFi.begin(cfg.staSsid, cfg.staPass);
  lastStaRetryMs = millis();
  Serial.printf("[STA] connecting to %s\n", cfg.staSsid);
}

void maintainStation() {
  wl_status_t s = WiFi.status();
  if (s != lastStaStatus) {
    lastStaStatus = s;
    if (s == WL_CONNECTED) {
      configureApDns();
      Serial.printf("[STA] connected: %s RSSI=%d DNS=%s\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.RSSI(),
                    WiFi.dnsIP(0).toString().c_str());
    } else {
      Serial.printf("[STA] status=%d\n", static_cast<int>(s));
    }
  }

  if (strlen(cfg.staSsid) > 0 && s != WL_CONNECTED) {
    uint32_t now = millis();
    if (now - lastStaRetryMs >= STA_RETRY_MS) {
      lastStaRetryMs = now;
      Serial.println(F("[STA] retry"));
      WiFi.reconnect();
    }
  }
}

// ----------------------------- Main browser UI -----------------------------

static const char MAIN_HTML[] PROGMEM = R"BTHTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#111">
<title>BlazeTube</title>
<style>
:root{color-scheme:dark;--bg:#0f0f0f;--panel:#181818;--line:#2c2c2c;--text:#f1f1f1;--muted:#aaa;--accent:#fff;--danger:#ff6b6b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font-family:Arial,Helvetica,sans-serif}
button,input{font:inherit}
header{position:sticky;top:0;z-index:10;background:#111;border-bottom:1px solid var(--line)}
.top{height:54px;display:flex;align-items:center;gap:8px;padding:7px 10px}
.brand{font-weight:700;white-space:nowrap}.brand b{font-size:11px;color:#999;margin-left:4px}
.spacer{flex:1}
.iconbtn,.tab,.ctl,.searchBtn,.homeBtn{border:1px solid var(--line);background:#222;color:#fff;border-radius:9px;min-height:38px;padding:0 12px}
.iconbtn:active,.tab:active,.ctl:active,.searchBtn:active,.homeBtn:active{transform:scale(.98)}
.status{font-size:11px;color:var(--muted);max-width:120px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.tabs{display:flex;gap:6px;padding:0 10px 8px}
.tab{flex:1}.tab.active{background:#eee;color:#111}
main{max-width:900px;margin:auto;padding:10px}
.notice{display:none;padding:10px 12px;border:1px solid #574b1c;background:#28230f;border-radius:9px;margin-bottom:10px;color:#ffe69a;font-size:13px}
.search{display:flex;gap:6px;margin-bottom:10px}
.search input{flex:1;min-width:0;background:#181818;color:#fff;border:1px solid var(--line);border-radius:10px;padding:11px 12px}
.playerWrap{display:none;background:#000;border-radius:12px;overflow:hidden;margin-bottom:8px;aspect-ratio:16/9}
#player{width:100%;height:100%}
.videoTitle{font-size:15px;font-weight:700;margin:8px 2px;display:none}
.controls{display:none;grid-template-columns:auto auto 1fr auto auto;gap:6px;align-items:center;margin-bottom:12px}
.ctl{padding:0 11px}.time{font-size:11px;color:var(--muted);min-width:78px;text-align:center}
input[type=range]{width:100%}
.results{display:grid;grid-template-columns:1fr;gap:8px}
.card{width:100%;display:grid;grid-template-columns:145px 1fr;gap:10px;text-align:left;border:0;border-bottom:1px solid var(--line);background:transparent;color:#fff;padding:8px 0}
.card img{width:145px;aspect-ratio:16/9;object-fit:cover;border-radius:9px;background:#222}
.ctitle{font-size:14px;font-weight:700;line-height:1.25;max-height:3.75em;overflow:hidden}
.cmeta{font-size:12px;color:#aaa;margin-top:7px}
.empty{padding:28px 12px;text-align:center;color:#aaa}
footer{padding:18px 8px 30px;text-align:center;color:#666;font-size:11px}
@media(min-width:700px){.results{grid-template-columns:1fr 1fr}.card{grid-template-columns:170px 1fr}.card img{width:170px}}
</style>
</head>
<body>
<header>
  <div class="top">
    <div class="brand">BlazeTube <b>shared</b></div>
    <div class="spacer"></div>
    <div id="status" class="status">Connecting…</div>
    <button class="iconbtn" onclick="hardRefresh()" title="Refresh">↻</button>
    <button class="iconbtn" onclick="location.href='/settings'" title="Settings">⚙</button>
  </div>
  <div class="tabs">
    <button id="tab0" class="tab" onclick="selectTab(0)">Tab 1</button>
    <button id="tab1" class="tab" onclick="selectTab(1)">Tab 2</button>
    <button id="tab2" class="tab" onclick="selectTab(2)">Tab 3</button>
  </div>
</header>

<main>
  <div id="notice" class="notice"></div>
  <form class="search" onsubmit="userSearch(event)">
    <button type="button" class="homeBtn" onclick="goHome()">⌂</button>
    <input id="q" autocomplete="off" placeholder="Search YouTube">
    <button class="searchBtn" type="submit">Search</button>
  </form>

  <div id="playerWrap" class="playerWrap"><div id="player"></div></div>
  <div id="videoTitle" class="videoTitle"></div>

  <div id="controls" class="controls">
    <button class="ctl" onclick="jump(-10)">−10</button>
    <button id="playBtn" class="ctl" onclick="togglePlay()">▶</button>
    <input id="seek" type="range" min="0" max="100" value="0" step="0.1"
           oninput="scrubbing=true" onchange="commitSeek(this.value)">
    <div id="time" class="time">0:00 / 0:00</div>
    <button class="ctl" onclick="jump(10)">+10</button>
  </div>

  <div id="results" class="results"><div class="empty">Loading session…</div></div>
  <footer>BlazeTube ESP8266 • the ESP owns the shared session</footer>
</main>

<script>
const $=id=>document.getElementById(id);
let clientCfg={hasKey:false,region:'PH',maxResults:8};
let state=null,lastRev=-1,currentVideo='',lastQuery=null;
let player=null,playerReady=false,ytReady=false,scrubbing=false,unlocked=false,applyingRemote=false;
let pollBusy=false,searchSeq=0,autoplayBlocked=false;
let clientId='c'+Math.random().toString(36).slice(2,12);
try{
  const old=localStorage.getItem('blazetubeClientId');
  if(old)clientId=old;
  else localStorage.setItem('blazetubeClientId',clientId);
}catch(e){}

document.addEventListener('pointerdown',()=>{unlocked=true},{passive:true});
document.addEventListener('touchstart',()=>{unlocked=true},{passive:true});

function notice(msg){
  const n=$('notice');
  if(!msg){n.style.display='none';n.textContent='';return}
  n.textContent=msg;n.style.display='block';
}
function showEmpty(msg){
  const box=$('results');box.innerHTML='';
  const d=document.createElement('div');d.className='empty';d.textContent=msg;box.appendChild(d);
}
function fmt(s){
  s=Math.max(0,Math.floor(Number(s)||0));
  const h=Math.floor(s/3600),m=Math.floor((s%3600)/60),x=s%60;
  return h?`${h}:${String(m).padStart(2,'0')}:${String(x).padStart(2,'0')}`:`${m}:${String(x).padStart(2,'0')}`;
}
async function postCmd(action,data={}){
  const p=new URLSearchParams({action,...data});
  try{
    const r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p,cache:'no-store'});
    if(!r.ok) throw new Error(await r.text());
    await poll(true);
  }catch(e){notice('Command failed: '+e.message)}
}
function selectTab(n){postCmd('select_tab',{tab:n})}
function goHome(){postCmd('home',{tab:state?state.active:0})}
function hardRefresh(){
  if(!state)return;
  lastQuery=null;
  postCmd('refresh_results',{tab:state.active});
}
function userSearch(e){
  e.preventDefault();
  const q=$('q').value.trim();
  if(!q)return;
  postCmd('search',{tab:state?state.active:0,q});
}
function openVideo(id,title){
  postCmd('open_video',{tab:state?state.active:0,vid:id,title:title||'',q:$('q').value.trim()});
}
function togglePlay(){
  if(!state)return;
  unlocked=true;
  const t=state.tabs[state.active];
  postCmd(t.playing?'pause':'play',{tab:state.active,pos:localPosition()});
}
function jump(delta){
  if(!state)return;
  unlocked=true;
  const p=Math.max(0,localPosition()+delta);
  postCmd('seek',{tab:state.active,pos:p});
}
function commitSeek(v){
  scrubbing=false;unlocked=true;
  if(!state)return;
  let dur=playerReady?Number(player.getDuration()||0):0;
  let p=dur?dur*(Number(v)/100):Number(v);
  postCmd('seek',{tab:state.active,pos:p});
}
function localPosition(){
  try{if(playerReady)return Number(player.getCurrentTime()||0)}catch(e){}
  if(!state)return 0;
  return Number(state.tabs[state.active].pos||0);
}

window.onYouTubeIframeAPIReady=()=>{ytReady=true;if(state)applyState(true)};

function createPlayer(t){
  if(!ytReady||!t.v)return;
  currentVideo=t.v;
  playerReady=false;
  player=new YT.Player('player',{
    width:'100%',height:'100%',videoId:t.v,
    playerVars:{
      controls:0,playsinline:1,rel:0,enablejsapi:1,origin:location.origin,
      start:Math.max(0,Math.floor(Number(t.pos)||0))
    },
    events:{
      onReady:ev=>{
        playerReady=true;
        try{ev.target.seekTo(Number(t.pos)||0,true)}catch(e){}
        syncPlayback(t,true);
      },
      onStateChange:ev=>{
        if(ev.data===YT.PlayerState.ENDED && state){
          postCmd('pause',{tab:state.active,pos:localPosition()});
        }
      },
      onAutoplayBlocked:()=>{
        autoplayBlocked=true;
        notice('This browser blocked remote autoplay. Tap Play once on this phone; synchronization will continue after that.');
      }
    }
  });
}
function destroyPlayer(){
  try{if(player)player.destroy()}catch(e){}
  player=null;playerReady=false;currentVideo='';
  $('playerWrap').innerHTML='<div id="player"></div>';
}
function syncPlayback(t,force){
  if(!playerReady)return;
  const expected=Number(t.pos)||0;
  let local=0;
  try{local=Number(player.getCurrentTime()||0)}catch(e){}
  if(force||Math.abs(local-expected)>2.5){
    applyingRemote=true;
    try{player.seekTo(expected,true)}catch(e){}
    setTimeout(()=>applyingRemote=false,250);
  }
  try{
    const ps=player.getPlayerState();
    if(t.playing){
      if(ps!==YT.PlayerState.PLAYING && unlocked){
        autoplayBlocked=false;
        player.playVideo();
      }else if(!unlocked){
        notice('Shared playback is active. Tap this page once if this phone does not start playing automatically.');
      }
    }else if(ps===YT.PlayerState.PLAYING || ps===YT.PlayerState.BUFFERING){
      player.pauseVideo();
    }
  }catch(e){}
}
function applyState(force=false){
  if(!state)return;
  document.querySelectorAll('.tab').forEach((b,i)=>b.classList.toggle('active',i===state.active));
  const t=state.tabs[state.active];
  if(document.activeElement!==$('q')) $('q').value=t.q||'';

  if(t.v){
    $('playerWrap').style.display='block';
    $('controls').style.display='grid';
    $('videoTitle').style.display='block';
    $('videoTitle').textContent=t.title||'YouTube video';
    if(currentVideo!==t.v){
      destroyPlayer();
      if(ytReady)createPlayer(t);
    }else{
      syncPlayback(t,force);
    }
  }else{
    $('playerWrap').style.display='none';
    $('controls').style.display='none';
    $('videoTitle').style.display='none';
    if(currentVideo)destroyPlayer();
  }

  $('playBtn').textContent=t.playing?'❚❚':'▶';

  if(lastQuery!==t.q||force){
    lastQuery=t.q;
    loadSharedResults();
  }
}
async function loadClientCfg(){
  try{
    const r=await fetch('/api/client-config',{cache:'no-store'});
    clientCfg=await r.json();
  }catch(e){}
}
async function publishResults(tab,items,error=''){
  const p=new URLSearchParams({tab:String(tab),client:clientId,count:String(items.length),error});
  items.slice(0,12).forEach((x,i)=>{
    p.set('id'+i,x.id||'');
    p.set('title'+i,x.title||'');
    p.set('channel'+i,x.channel||'');
  });
  const r=await fetch('/api/results',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p,cache:'no-store'});
  if(!r.ok)throw new Error(await r.text());
}
async function fetchGrantedResults(g,tab,seq){
  let u='';
  if(g.kind==='search'){
    u='https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&videoEmbeddable=true&safeSearch=moderate'
      +'&maxResults='+encodeURIComponent(g.maxResults)
      +'&regionCode='+encodeURIComponent(g.region)
      +'&q='+encodeURIComponent(g.q)
      +'&key='+encodeURIComponent(g.key);
  }else{
    u='https://www.googleapis.com/youtube/v3/videos?part=snippet&chart=mostPopular'
      +'&maxResults='+encodeURIComponent(g.maxResults)
      +'&regionCode='+encodeURIComponent(g.region)
      +'&key='+encodeURIComponent(g.key);
  }
  try{
    const r=await fetch(u);
    const j=await r.json();
    if(seq!==searchSeq)return;
    if(!r.ok)throw new Error(j?.error?.message||('HTTP '+r.status));
    let items=[];
    if(g.kind==='search'){
      items=(j.items||[]).map(x=>({id:x.id?.videoId,title:x.snippet?.title||'',channel:x.snippet?.channelTitle||''})).filter(x=>x.id);
    }else{
      items=(j.items||[]).map(x=>({id:x.id,title:x.snippet?.title||'',channel:x.snippet?.channelTitle||''})).filter(x=>x.id);
    }
    await publishResults(tab,items,'');
  }catch(e){
    try{await publishResults(tab,[],String(e.message||e).slice(0,90))}catch(_){}
  }
}
async function loadSharedResults(){
  if(!state)return;
  const tab=state.active;
  const wantedQuery=state.tabs[tab].q||'';
  const seq=++searchSeq;

  if(!clientCfg.hasKey){
    showEmpty('Add a YouTube Data API v3 key in Settings to enable browsing/search.');
    notice('Internet repeating and shared playback work without an API key, but YouTube browsing/search needs a YouTube Data API v3 key.');
    return;
  }

  showEmpty(wantedQuery?'Searching shared results…':'Loading shared home feed…');
  notice('');

  for(let attempt=0;attempt<25;attempt++){
    if(seq!==searchSeq||!state||state.active!==tab||(state.tabs[tab].q||'')!==wantedQuery)return;
    try{
      const r=await fetch('/api/results?tab='+tab+'&client='+encodeURIComponent(clientId)+'&x='+Date.now(),{cache:'no-store'});
      const j=await r.json();
      if(!r.ok)throw new Error(j?.error||('HTTP '+r.status));

      if(j.state==='ready'){
        if(j.error){
          showEmpty('YouTube API error: '+j.error);
          return;
        }
        renderResults((j.items||[]).map(x=>({id:x.id,title:x.title||'',channel:x.channel||''})));
        return;
      }

      if(j.state==='fetch'){
        await fetchGrantedResults(j,tab,seq);
        await new Promise(r=>setTimeout(r,120));
        continue;
      }

      if(j.state==='no-key'){
        showEmpty('Add a YouTube Data API v3 key in Settings.');
        return;
      }

      await new Promise(r=>setTimeout(r,450));
    }catch(e){
      showEmpty('Result sync error: '+String(e.message||e));
      return;
    }
  }
  showEmpty('Waiting for another connected device to finish loading these results…');
}
function renderResults(items){
  const box=$('results');box.innerHTML='';
  if(!items.length){showEmpty('No videos found.');return}
  for(const x of items){
    if(!x.id)continue;
    const b=document.createElement('button');b.type='button';b.className='card';
    const img=document.createElement('img');
    img.loading='lazy';
    img.referrerPolicy='no-referrer';
    img.src='https://i.ytimg.com/vi/'+encodeURIComponent(x.id)+'/mqdefault.jpg';
    const text=document.createElement('div');
    const title=document.createElement('div');title.className='ctitle';title.textContent=x.title||'Untitled';
    const meta=document.createElement('div');meta.className='cmeta';meta.textContent=x.channel||'YouTube';
    text.append(title,meta);b.append(img,text);
    b.onclick=()=>openVideo(x.id,x.title||'');
    box.appendChild(b);
  }
}
async function poll(force=false){
  if(pollBusy&&!force)return;
  pollBusy=true;
  try{
    const r=await fetch('/api/session?x='+Date.now(),{cache:'no-store'});
    if(!r.ok)throw new Error('HTTP '+r.status);
    const j=await r.json();
    $('status').textContent=j.net?.online?('Online • '+j.net.clients+' device'+(j.net.clients===1?'':'s')):'No upstream Internet';
    const changed=force||j.rev!==lastRev||(state&&j.active!==state.active);
    state=j;lastRev=j.rev;
    applyState(changed);
  }catch(e){
    $('status').textContent='ESP reconnecting…';
  }finally{pollBusy=false}
}
function tick(){
  if(!state)return;
  const t=state.tabs[state.active];
  let p=Number(t.pos)||0,d=0;
  try{if(playerReady){p=Number(player.getCurrentTime()||p);d=Number(player.getDuration()||0)}}catch(e){}
  $('time').textContent=fmt(p)+' / '+fmt(d);
  if(!scrubbing){
    $('seek').max=100;
    $('seek').value=d?Math.min(100,(p/d)*100):0;
  }
}

(async()=>{
  await loadClientCfg();
  const tag=document.createElement('script');
  tag.src='https://www.youtube.com/iframe_api';
  document.head.appendChild(tag);
  await poll(true);
  setInterval(()=>poll(false),900);
  setInterval(tick,500);
})();
</script>
</body>
</html>
)BTHTML";

// ----------------------------- Settings page -----------------------------

void handleSettings() {
  if (!requireAdmin()) return;

  String h;
  h.reserve(8000);
  h += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  h += F("<title>BlazeTube Settings</title><style>");
  h += F("body{margin:0;background:#101010;color:#eee;font-family:Arial,sans-serif}main{max-width:720px;margin:auto;padding:18px}");
  h += F(".box{background:#191919;border:1px solid #333;border-radius:12px;padding:16px;margin:12px 0}h1{font-size:22px}h2{font-size:16px;margin-top:0}");
  h += F("label{display:block;margin:11px 0 5px;color:#bbb;font-size:13px}input{width:100%;padding:11px;background:#0f0f0f;color:#fff;border:1px solid #444;border-radius:8px}");
  h += F("button,a.btn{display:inline-block;padding:11px 14px;border:0;border-radius:8px;background:#eee;color:#111;text-decoration:none;font-weight:700;margin:4px 4px 4px 0}");
  h += F(".danger{background:#8b2727!important;color:#fff!important}.muted{font-size:12px;color:#999;line-height:1.45}.ok{color:#8fda8f}.bad{color:#ff8b8b}");
  h += F("</style></head><body><main><a class='btn' href='/'>← Browser</a><h1>BlazeTube Settings</h1>");

  h += F("<div class='box'><h2>Status</h2><div>");
  h += F("Firmware: "); h += FW_VERSION; h += F("<br>AP: "); h += htmlEscape(cfg.apSsid);
  h += F("<br>AP IP: "); h += WiFi.softAPIP().toString();
  h += F("<br>Connected AP clients: "); h += String(WiFi.softAPgetStationNum());
  h += F("<br>Upstream: ");
  if (WiFi.status() == WL_CONNECTED) {
    h += F("<span class='ok'>connected</span> to ");
    h += htmlEscape(cfg.staSsid);
    h += F("<br>STA IP: "); h += WiFi.localIP().toString();
    h += F("<br>RSSI: "); h += String(WiFi.RSSI()); h += F(" dBm");
  } else {
    h += F("<span class='bad'>not connected</span>");
  }
  h += F("<br>NAPT repeater: ");
  h += naptReady ? F("<span class='ok'>enabled</span>") : F("<span class='bad'>not available</span>");
  h += F("<br>Free heap: "); h += String(ESP.getFreeHeap()); h += F(" bytes");
  h += F("</div></div>");

  h += F("<form method='post' action='/api/settings'>");

  h += F("<div class='box'><h2>Upstream Wi‑Fi (STA)</h2>");
  h += F("<label>Wi‑Fi SSID</label><input name='sta_ssid' maxlength='32' value='");
  h += htmlEscape(cfg.staSsid); h += F("'>");
  h += F("<label>Password</label><input name='sta_pass' type='password' maxlength='64' placeholder='Leave blank to keep existing password'>");
  h += F("<label><input style='width:auto' type='checkbox' name='sta_clear' value='1'> Clear stored STA password / use open network</label>");
  h += F("</div>");

  h += F("<div class='box'><h2>Repeater Access Point</h2>");
  h += F("<label>AP SSID</label><input name='ap_ssid' maxlength='32' value='");
  h += htmlEscape(cfg.apSsid); h += F("'>");
  h += F("<label>AP password</label><input name='ap_pass' type='password' minlength='8' maxlength='64' placeholder='Leave blank to keep existing password'>");
  h += F("<div class='muted'>The repeater AP is intentionally WPA2 protected. Minimum password length: 8.</div>");
  h += F("</div>");

  h += F("<div class='box'><h2>YouTube</h2>");
  h += F("<label>YouTube Data API v3 key</label><input name='yt_key' type='password' maxlength='64' placeholder='");
  h += strlen(cfg.ytApiKey) ? F("Key already stored — leave blank to keep it") : F("Paste API key");
  h += F("'>");
  h += F("<label><input style='width:auto' type='checkbox' name='yt_clear' value='1'> Clear stored API key</label>");
  h += F("<label>Region code</label><input name='region' maxlength='2' value='"); h += htmlEscape(cfg.region); h += F("'>");
  h += F("<label>Videos per page (5–12)</label><input name='max_results' type='number' min='5' max='12' value='"); h += String(cfg.maxResults); h += F("'>");
  h += F("<div class='muted'>Search and home browsing require the API key. Playback uses YouTube's official iframe player.</div>");
  h += F("</div>");

  h += F("<div class='box'><h2>System</h2>");
  h += F("<label>Hostname</label><input name='hostname' maxlength='24' value='"); h += htmlEscape(cfg.hostname); h += F("'>");
  h += F("<label>Admin password</label><input name='admin_pass' type='password' minlength='8' maxlength='32' placeholder='Leave blank to keep existing admin password'>");
  h += F("<div class='muted'>Admin username is always <b>admin</b>. Saving network settings reboots the ESP8266.</div>");
  h += F("</div>");

  h += F("<button type='submit'>Save & Reboot</button></form>");

  h += F("<div class='box'><h2>Maintenance</h2>");
  h += F("<form method='post' action='/api/reboot' style='display:inline'><button type='submit'>Reboot</button></form>");
  h += F("<form method='post' action='/api/factory-reset' style='display:inline' onsubmit=\"return confirm('Erase all settings and the shared session?')\"><button class='danger' type='submit'>Factory Reset</button></form>");
  h += F("</div>");

  h += F("<div class='muted'>Direct AP address: http://192.168.77.1/ &nbsp; • &nbsp; mDNS: http://");
  h += htmlEscape(cfg.hostname); h += F(".local/</div>");
  h += F("</main></body></html>");

  noCache();
  server.send(200, F("text/html; charset=utf-8"), h);
}

// ----------------------------- API handlers -----------------------------

void handleRoot() {
  noCache();
  server.send_P(200, PSTR("text/html; charset=utf-8"), MAIN_HTML);
}

void handleClientConfig() {
  String out;
  out.reserve(120);
  out += F("{\"hasKey\":");
  out += strlen(cfg.ytApiKey) ? F("true") : F("false");
  out += F(",\"region\":\"");
  out += jsonEscape(cfg.region);
  out += F("\",\"maxResults\":");
  out += String(cfg.maxResults);
  out += F("}");
  noCache();
  server.send(200, F("application/json"), out);
}

void handleStatus() {
  String out;
  out.reserve(420);
  out += F("{\"fw\":\""); out += FW_VERSION;
  out += F("\",\"heap\":"); out += String(ESP.getFreeHeap());
  out += F(",\"uptime\":"); out += String(millis());
  out += F(",\"nat\":"); out += naptReady ? F("true") : F("false");
  out += F(",\"ap\":{\"ssid\":\""); out += jsonEscape(cfg.apSsid);
  out += F("\",\"ip\":\""); out += WiFi.softAPIP().toString();
  out += F("\",\"clients\":"); out += String(WiFi.softAPgetStationNum()); out += F("}");
  out += F(",\"sta\":{\"connected\":"); out += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
  out += F(",\"ssid\":\""); out += jsonEscape(cfg.staSsid);
  out += F("\",\"ip\":\""); out += WiFi.localIP().toString();
  out += F("\",\"rssi\":"); out += String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  out += F("}}");
  noCache();
  server.send(200, F("application/json"), out);
}

void handleSession() {
  uint32_t now = millis();
  String out;
  out.reserve(1500);

  out += F("{\"rev\":"); out += String(sess.revision);
  out += F(",\"serverMs\":"); out += String(now);
  out += F(",\"active\":"); out += String(sess.activeTab);
  out += F(",\"net\":{\"online\":"); out += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
  out += F(",\"clients\":"); out += String(WiFi.softAPgetStationNum()); out += F("}");
  out += F(",\"tabs\":[");

  for (uint8_t i = 0; i < TAB_COUNT; i++) {
    if (i) out += ',';
    const TabState& t = sess.tabs[i];
    out += F("{\"v\":\""); out += jsonEscape(t.videoId);
    out += F("\",\"title\":\""); out += jsonEscape(t.title);
    out += F("\",\"q\":\""); out += jsonEscape(t.query);
    out += F("\",\"pos\":"); out += String(effectivePosition(t, now), 2);
    out += F(",\"playing\":"); out += t.playing ? F("true") : F("false");
    out += F("}");
  }
  out += F("]}");

  noCache();
  server.send(200, F("application/json"), out);
}

int parseTabArg() {
  if (!server.hasArg("tab")) return -1;
  int t = server.arg("tab").toInt();
  if (t < 0 || t >= TAB_COUNT) return -1;
  return t;
}

bool validClientToken(const String& c) {
  if (c.length() < 1 || c.length() > 16) return false;
  for (size_t i = 0; i < c.length(); i++) {
    char ch = c[i];
    if (!isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-') return false;
  }
  return true;
}

void handleResultsGet() {
  noCache();
  int tab = parseTabArg();
  String client = server.arg("client");
  if (tab < 0 || !validClientToken(client)) {
    server.send(400, F("application/json"), F("{\"error\":\"invalid tab or client\"}"));
    return;
  }

  if (strlen(cfg.ytApiKey) == 0) {
    server.send(200, F("application/json"), F("{\"state\":\"no-key\"}"));
    return;
  }

  ensureResultCacheIdentity(static_cast<uint8_t>(tab));
  SearchCache& c = resultCache[tab];
  uint32_t now = millis();

  if (c.state == 1 && resultLeaseExpired(c, now)) {
    c.state = 0;
    c.leaseClient[0] = '\0';
    c.leaseUntilMs = 0;
  }

  if (c.state == 2) {
    String out;
    out.reserve(3600);
    out += F("{\"state\":\"ready\",\"error\":\"");
    out += jsonEscape(c.error);
    out += F("\",\"items\":[");
    for (uint8_t i = 0; i < c.count; i++) {
      if (i) out += ',';
      out += F("{\"id\":\""); out += jsonEscape(c.items[i].videoId);
      out += F("\",\"title\":\""); out += jsonEscape(c.items[i].title);
      out += F("\",\"channel\":\""); out += jsonEscape(c.items[i].channel);
      out += F("\"}");
    }
    out += F("]}");
    noCache();
    server.send(200, F("application/json"), out);
    return;
  }

  if (c.state == 0) {
    c.state = 1;
    c.leaseUntilMs = now + 15000UL;
    copyText(c.leaseClient, client);

    String out;
    out.reserve(320);
    out += F("{\"state\":\"fetch\",\"kind\":\"");
    out += c.kind == 2 ? F("search") : F("home");
    out += F("\",\"q\":\""); out += jsonEscape(c.query);
    out += F("\",\"key\":\""); out += jsonEscape(cfg.ytApiKey);
    out += F("\",\"region\":\""); out += jsonEscape(cfg.region);
    out += F("\",\"maxResults\":"); out += String(cfg.maxResults);
    out += F("}");
    noCache();
    server.send(200, F("application/json"), out);
    return;
  }

  server.send(200, F("application/json"), F("{\"state\":\"wait\"}"));
}

void handleResultsPost() {
  int tab = parseTabArg();
  String client = server.arg("client");
  if (tab < 0 || !validClientToken(client)) {
    server.send(400, F("text/plain"), F("Invalid tab or client"));
    return;
  }

  ensureResultCacheIdentity(static_cast<uint8_t>(tab));
  SearchCache& c = resultCache[tab];
  uint32_t now = millis();

  if (c.state != 1 || resultLeaseExpired(c, now) || client != String(c.leaseClient)) {
    server.send(409, F("text/plain"), F("Search lease expired or belongs to another client"));
    return;
  }

  int count = server.arg("count").toInt();
  if (count < 0) count = 0;
  if (count > 12) count = 12;
  if (count > cfg.maxResults) count = cfg.maxResults;

  memset(c.items, 0, sizeof(c.items));
  c.count = 0;
  c.error[0] = '\0';

  String err = server.arg("error");
  err.trim();
  if (err.length() > 0) {
    copyText(c.error, err);
  } else {
    for (int i = 0; i < count; i++) {
      String suffix = String(i);
      String id = server.arg(String(F("id")) + suffix);
      if (!validVideoId(id)) continue;

      SearchResultItem& item = c.items[c.count];
      copyText(item.videoId, id);
      copyText(item.title, server.arg(String(F("title")) + suffix));
      copyText(item.channel, server.arg(String(F("channel")) + suffix));
      c.count++;
      if (c.count >= 12) break;
    }
  }

  c.state = 2;
  c.leaseClient[0] = '\0';
  c.leaseUntilMs = 0;

  server.send(200, F("application/json"), F("{\"ok\":true}"));
}

void sendCmdOk() {
  server.send(200, F("application/json"), F("{\"ok\":true}"));
}

void handleCommand() {
  String action = server.arg("action");
  int tab = parseTabArg();
  uint32_t now = millis();

  if (action == F("select_tab")) {
    if (tab < 0) { server.send(400, F("text/plain"), F("Invalid tab")); return; }
    if (sess.activeTab != static_cast<uint8_t>(tab)) {
      sess.activeTab = static_cast<uint8_t>(tab);
      markSessionChanged();
    }
    sendCmdOk();
    return;
  }

  if (tab < 0) {
    server.send(400, F("text/plain"), F("Invalid tab"));
    return;
  }

  TabState& t = sess.tabs[tab];

  if (action == F("home")) {
    normalizeTab(t, now);
    memset(t.videoId, 0, sizeof(t.videoId));
    memset(t.title, 0, sizeof(t.title));
    memset(t.query, 0, sizeof(t.query));
    t.basePosition = 0;
    t.baseAtMs = now;
    t.playing = 0;
    sess.activeTab = tab;
    invalidateResultCache(static_cast<uint8_t>(tab));
    markSessionChanged();
    sendCmdOk();
    return;
  }

  if (action == F("search")) {
    String q = server.arg("q");
    q.trim();
    if (q.length() == 0) { server.send(400, F("text/plain"), F("Empty search")); return; }
    copyText(t.query, q);
    memset(t.videoId, 0, sizeof(t.videoId));
    memset(t.title, 0, sizeof(t.title));
    t.basePosition = 0;
    t.baseAtMs = now;
    t.playing = 0;
    sess.activeTab = tab;
    invalidateResultCache(static_cast<uint8_t>(tab));
    markSessionChanged();
    sendCmdOk();
    return;
  }

  if (action == F("refresh_results")) {
    invalidateResultCache(static_cast<uint8_t>(tab));
    sess.activeTab = tab;
    markSessionChanged();
    sendCmdOk();
    return;
  }

  if (action == F("open_video")) {
    String id = server.arg("vid");
    if (!validVideoId(id)) { server.send(400, F("text/plain"), F("Invalid YouTube video ID")); return; }
    copyText(t.videoId, id);
    copyText(t.title, server.arg("title"));
    if (server.hasArg("q")) copyText(t.query, server.arg("q"));
    t.basePosition = 0;
    t.baseAtMs = now;
    t.playing = 0;
    sess.activeTab = tab;
    markSessionChanged();
    sendCmdOk();
    return;
  }

  if (action == F("play") || action == F("pause") || action == F("seek")) {
    normalizeTab(t, now);
    if (server.hasArg("pos")) {
      float p = server.arg("pos").toFloat();
      if (!(p >= 0.0f)) p = 0.0f;  // also catches NaN
      if (p > 86400.0f) p = 86400.0f;
      t.basePosition = p;
      t.baseAtMs = now;
    }
    if (action == F("play")) t.playing = 1;
    if (action == F("pause")) t.playing = 0;
    sess.activeTab = tab;
    markSessionChanged();
    sendCmdOk();
    return;
  }

  server.send(400, F("text/plain"), F("Unknown command"));
}

void handleSaveSettings() {
  if (!requireAdmin()) return;

  String newSta = server.arg("sta_ssid");
  newSta.trim();
  if (newSta.length() > 32) { server.send(400, F("text/plain"), F("STA SSID too long")); return; }

  String newAp = server.arg("ap_ssid");
  newAp.trim();
  if (newAp.length() < 1 || newAp.length() > 32) {
    server.send(400, F("text/plain"), F("AP SSID must be 1-32 bytes"));
    return;
  }

  String host = server.arg("hostname");
  host.trim();
  if (!validHostname(host)) {
    server.send(400, F("text/plain"), F("Hostname must be 1-24 letters, numbers, or hyphens"));
    return;
  }

  String region = server.arg("region");
  region.trim();
  region.toUpperCase();
  if (!validRegion(region)) {
    server.send(400, F("text/plain"), F("Region must be a two-letter country code, e.g. PH"));
    return;
  }

  int mr = server.arg("max_results").toInt();
  if (mr < 5 || mr > 12) mr = 8;

  String apPass = server.arg("ap_pass");
  if (apPass.length() > 0 && (apPass.length() < 8 || apPass.length() > 64)) {
    server.send(400, F("text/plain"), F("AP password must be 8-64 characters"));
    return;
  }

  String adminPass = server.arg("admin_pass");
  if (adminPass.length() > 0 && (adminPass.length() < 8 || adminPass.length() > 32)) {
    server.send(400, F("text/plain"), F("Admin password must be 8-32 characters"));
    return;
  }

  copyText(cfg.staSsid, newSta);
  if (server.hasArg("sta_clear")) {
    cfg.staPass[0] = '\0';
  } else if (server.arg("sta_pass").length() > 0) {
    copyText(cfg.staPass, server.arg("sta_pass"));
  }

  copyText(cfg.apSsid, newAp);
  if (apPass.length() > 0) copyText(cfg.apPass, apPass);

  if (server.hasArg("yt_clear")) {
    cfg.ytApiKey[0] = '\0';
  } else if (server.arg("yt_key").length() > 0) {
    copyText(cfg.ytApiKey, server.arg("yt_key"));
  }

  copyText(cfg.hostname, host);
  copyText(cfg.region, region);
  cfg.maxResults = static_cast<uint8_t>(mr);
  if (adminPass.length() > 0) copyText(cfg.adminPass, adminPass);

  if (!saveConfig()) {
    server.send(500, F("text/plain"), F("Failed to save configuration"));
    return;
  }

  saveSession();
  server.send(200, F("text/html; charset=utf-8"),
              F("<!doctype html><meta name=viewport content='width=device-width'><body style='font-family:Arial;background:#111;color:#eee;padding:30px'>"
                "<h2>Saved.</h2><p>BlazeTube is rebooting. Reconnect to its AP if the AP name/password changed.</p></body>"));
  delay(600);
  ESP.restart();
}

void handleReboot() {
  if (!requireAdmin()) return;
  saveSession();
  server.send(200, F("text/plain"), F("Rebooting..."));
  delay(300);
  ESP.restart();
}

void handleFactoryReset() {
  if (!requireAdmin()) return;
  if (fsReady) {
    LittleFS.remove(CONFIG_FILE);
    LittleFS.remove(CONFIG_TMP);
    LittleFS.remove(SESSION_FILE);
    LittleFS.remove(SESSION_TMP);
  }
  server.send(200, F("text/plain"), F("Factory reset complete. Rebooting..."));
  delay(400);
  ESP.restart();
}

void handleNotFound() {
  // Keep accidental local requests user-friendly without hijacking Internet DNS.
  if (server.uri().startsWith(F("/api/"))) {
    server.send(404, F("application/json"), F("{\"error\":\"not found\"}"));
  } else {
    server.sendHeader(F("Location"), F("/"), true);
    server.send(302, F("text/plain"), F(""));
  }
}

void startHttp() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleSettings);

  server.on("/api/client-config", HTTP_GET, handleClientConfig);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/session", HTTP_GET, handleSession);
  server.on("/api/results", HTTP_GET, handleResultsGet);
  server.on("/api/results", HTTP_POST, handleResultsPost);
  server.on("/api/cmd", HTTP_POST, handleCommand);
  server.on("/api/settings", HTTP_POST, handleSaveSettings);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("[HTTP] server started on port 80"));
}

// ----------------------------- Arduino lifecycle -----------------------------

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("======================================"));
  Serial.printf("%s %s\n", FW_NAME, FW_VERSION);
  Serial.printf("Chip ID: %06X | Flash: %u bytes\n", ESP.getChipId(), ESP.getFlashChipRealSize());
  Serial.println(F("======================================"));

  fsReady = LittleFS.begin();
  Serial.printf("[FS] %s\n", fsReady ? "LittleFS mounted" : "LittleFS FAILED");

  setDefaults();
  bool firstBootCredentials = false;

  if (fsReady && loadConfig()) {
    Serial.println(F("[CFG] loaded"));
  } else {
    Serial.println(F("[CFG] creating first-boot configuration"));
    if (fsReady && !saveConfig()) Serial.println(F("[CFG] WARNING: could not persist first-boot configuration"));
    firstBootCredentials = true;
  }

  // Reset session defaults separately if no valid session exists.
  SessionData emptySession = sess;
  (void)emptySession;
  memset(&sess, 0, sizeof(sess));
  sess.magic = SESSION_MAGIC;
  sess.version = SESSION_VERSION;
  sess.revision = 1;
  sess.activeTab = 0;

  if (fsReady && loadSession()) {
    Serial.println(F("[SESSION] restored"));
  } else {
    Serial.println(F("[SESSION] new"));
  }

  WiFi.persistent(false);                 // prevent SDK from wearing flash with Wi-Fi config writes
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);     // better repeater latency/stability
  delay(50);

  startSoftAp();
  startNapt();
  startStation();

  startHttp();

  mdnsReady = MDNS.begin(cfg.hostname);
  if (mdnsReady) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mDNS] http://%s.local/\n", cfg.hostname);
  }

  Serial.printf("[READY] AP: %s\n", cfg.apSsid);
  Serial.printf("[READY] Web: http://%s/\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("[READY] Free heap: %u\n", ESP.getFreeHeap());
  if (firstBootCredentials) {
    Serial.println(F("[FIRST BOOT] Save these credentials, then change them in Settings:"));
    Serial.printf("[FIRST BOOT] AP password: %s\n", cfg.apPass);
    Serial.println(F("[FIRST BOOT] Admin username: admin"));
    Serial.printf("[FIRST BOOT] Admin password: %s\n", cfg.adminPass);
  }
}

void loop() {
  server.handleClient();
  if (mdnsReady) MDNS.update();
  maintainStation();

  if (sessionDirty && (millis() - lastSessionSaveMs >= SESSION_SAVE_MIN_MS)) {
    saveSession();
  }

  yield();
}