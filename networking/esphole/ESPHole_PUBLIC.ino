/*
  ESPHole - Pi-hole-inspired DNS sinkhole + ESP8266 Wi-Fi NAT repeater
  -------------------------------------------------------------------
  Target: ESP8266 Arduino Core 3.1.2 (NodeMCU / Wemos D1 mini / ESP-12E/F)

  What it does:
    - Creates an ESP8266 SoftAP (default: ESPHole-XXXXXX)
    - Connects that AP to an upstream Wi-Fi network using lwIP NAPT/NAT
    - Advertises the ESP8266 itself as DNS to SoftAP clients
    - Blocks configured domains (including subdomains) at DNS level
    - Forwards allowed DNS queries to the upstream DNS server
    - Provides a small DNS response cache and runtime query statistics
    - Stores Wi-Fi settings, blocklist, and allowlist in LittleFS
    - Provides a captive setup portal whenever upstream Wi-Fi is offline
    - Provides a web dashboard at http://10.42.0.1/

  First-boot access:
    - SoftAP/admin passwords are generated uniquely on first boot.
    - The generated credentials are saved to LittleFS.
    - Read them from Serial Monitor at 115200 during first-boot setup.
    - Admin username is: admin

  IMPORTANT LIMITS:
    - This is "Pi-hole-lite", not the Linux Pi-hole codebase.
    - ESP8266 RAM is small. The in-memory hashed blocklist is capped below.
    - DNS-over-HTTPS/TLS/QUIC can bypass normal port-53 DNS filtering.
    - NAT support requires an ESP8266 core/lwIP build with NAPT enabled.
    - Use ESP8266 Arduino Core 3.1.2 and an lwIP v2 option.
    - This sketch uses only libraries included with the ESP8266 Arduino core.

  License for this sketch: MIT
*/

#include <Arduino.h>
#include <stddef.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <LittleFS.h>

#if LWIP_FEATURES && !LWIP_IPV6
  #include <lwip/napt.h>
  #define ESPHOLE_HAS_NAPT 1
#else
  #define ESPHOLE_HAS_NAPT 0
#endif

// ----------------------------- Build options -----------------------------

static const char *ESPHOLE_VERSION = "1.0.0";

static const IPAddress AP_IP(10, 42, 0, 1);
static const IPAddress AP_GW(10, 42, 0, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

static const uint16_t DNS_PORT = 53;
static const uint16_t DNS_UPSTREAM_LOCAL_PORT = 53530;
static const uint16_t DNS_MAX_PACKET = 512;
static const uint16_t DNS_TIMEOUT_MS = 900;
static const uint32_t DNS_CACHE_MS = 30000UL;

static const uint16_t MAX_BLOCK_HASHES = 1400;  // ~5.6 KB
static const uint16_t MAX_ALLOW_HASHES = 256;   // ~1.0 KB
static const uint8_t  RECENT_QUERY_COUNT = 12;
static const uint8_t  DNS_CACHE_SLOTS = 4;

#if ESPHOLE_HAS_NAPT
static const uint16_t NAPT_ENTRIES = 256;
static const uint8_t  NAPT_PORTMAP_ENTRIES = 8;
#endif

static const char *CONFIG_FILE = "/config.bin";
static const char *BLOCK_FILE  = "/blocklist.txt";
static const char *ALLOW_FILE  = "/allowlist.txt";

// ----------------------------- Persistent config -----------------------------

static const uint32_t CONFIG_MAGIC = 0x45535048UL; // "ESPH"
static const uint16_t CONFIG_VERSION = 1;

struct __attribute__((packed)) Config {
  uint32_t magic;
  uint16_t version;
  uint8_t configured;
  uint8_t filterEnabled;

  char staSsid[33];
  char staPass[65];

  char apSsid[33];
  char apPass[65];

  char adminPass[33];

  uint8_t useCustomDns;
  uint8_t customDns[4];

  uint32_t crc;
};

Config cfg;

// ----------------------------- Runtime state -----------------------------

ESP8266WebServer server(80);
WiFiUDP dnsClientUdp;
WiFiUDP dnsUpstreamUdp;

uint32_t blockHashes[MAX_BLOCK_HASHES];
uint32_t allowHashes[MAX_ALLOW_HASHES];
uint16_t blockCount = 0;
uint16_t allowCount = 0;
bool blockListTruncated = false;
bool allowListTruncated = false;

uint32_t totalQueries = 0;
uint32_t blockedQueries = 0;
uint32_t forwardedQueries = 0;
uint32_t cacheHits = 0;
uint32_t upstreamFailures = 0;

struct RecentQuery {
  char domain[64];
  uint16_t qtype;
  uint8_t blocked;
  uint32_t whenMs;
};
RecentQuery recentQueries[RECENT_QUERY_COUNT];
uint8_t recentHead = 0;
uint8_t recentUsed = 0;

struct DnsCacheEntry {
  uint32_t key;
  uint16_t len;
  uint32_t expiresAt;
  uint8_t data[DNS_MAX_PACKET];
  uint8_t valid;
};
DnsCacheEntry dnsCache[DNS_CACHE_SLOTS];
uint8_t cacheReplaceIndex = 0;

bool naptReady = false;
bool naptInitAttempted = false;
wl_status_t lastStaStatus = WL_IDLE_STATUS;
uint32_t pendingRestartAt = 0;

// ----------------------------- Utility -----------------------------

static uint32_t crc32Bytes(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return ~crc;
}

static void safeCopy(char *dst, size_t dstSize, const String &src) {
  if (!dst || dstSize == 0) return;
  size_t n = src.length();
  if (n >= dstSize) n = dstSize - 1;
  memcpy(dst, src.c_str(), n);
  dst[n] = '\0';
}

static void safeCopyC(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  size_t n = strlen(src);
  if (n >= dstSize) n = dstSize - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static String htmlEscape(const String &s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}

static bool ipIsZero(const IPAddress &ip) {
  return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

static IPAddress configuredUpstreamDns() {
  if (cfg.useCustomDns) {
    return IPAddress(cfg.customDns[0], cfg.customDns[1], cfg.customDns[2], cfg.customDns[3]);
  }

  IPAddress ip = WiFi.dnsIP(0);
  if (!ipIsZero(ip)) return ip;

  // Safe fallback only if DHCP did not provide DNS.
  return IPAddress(1, 1, 1, 1);
}

static bool stationOnline() {
  return WiFi.status() == WL_CONNECTED;
}

static bool captiveMode() {
  // If the internet side is down, DNS points web checks to ESPHole so the
  // user can fix Wi-Fi settings without knowing the AP IP.
  return !stationOnline();
}

static String qtypeName(uint16_t t) {
  switch (t) {
    case 1: return F("A");
    case 2: return F("NS");
    case 5: return F("CNAME");
    case 6: return F("SOA");
    case 12: return F("PTR");
    case 15: return F("MX");
    case 16: return F("TXT");
    case 28: return F("AAAA");
    case 33: return F("SRV");
    case 65: return F("HTTPS");
    default: return String(t);
  }
}

// ----------------------------- Config I/O -----------------------------

static void setDefaultConfig() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CONFIG_MAGIC;
  cfg.version = CONFIG_VERSION;
  cfg.filterEnabled = 1;
  cfg.configured = 0;

  char ssid[33];
  snprintf(ssid, sizeof(ssid), "ESPHole-%06X", ESP.getChipId());
  safeCopyC(cfg.apSsid, sizeof(cfg.apSsid), ssid);
  // Generate per-device first-boot credentials instead of shipping one public password.
  // They are persisted by saveConfig() immediately after defaults are created.
  char apPass[25];
  char adminPass[25];
  snprintf(apPass, sizeof(apPass), "EH-%08lX-%04lX",
           static_cast<unsigned long>(ESP.random()),
           static_cast<unsigned long>(ESP.random() & 0xFFFFUL));
  snprintf(adminPass, sizeof(adminPass), "EHA-%08lX-%04lX",
           static_cast<unsigned long>(ESP.random()),
           static_cast<unsigned long>(ESP.random() & 0xFFFFUL));
  safeCopyC(cfg.apPass, sizeof(cfg.apPass), apPass);
  safeCopyC(cfg.adminPass, sizeof(cfg.adminPass), adminPass);

  cfg.useCustomDns = 0;
  cfg.customDns[0] = 1;
  cfg.customDns[1] = 1;
  cfg.customDns[2] = 1;
  cfg.customDns[3] = 1;
}

static bool saveConfig() {
  cfg.magic = CONFIG_MAGIC;
  cfg.version = CONFIG_VERSION;
  cfg.crc = crc32Bytes(reinterpret_cast<const uint8_t *>(&cfg), offsetof(Config, crc));

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return false;
  size_t written = f.write(reinterpret_cast<const uint8_t *>(&cfg), sizeof(cfg));
  f.close();
  return written == sizeof(cfg);
}

static bool loadConfig() {
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f || f.size() != sizeof(cfg)) {
    if (f) f.close();
    return false;
  }

  Config tmp;
  size_t got = f.read(reinterpret_cast<uint8_t *>(&tmp), sizeof(tmp));
  f.close();
  if (got != sizeof(tmp)) return false;

  if (tmp.magic != CONFIG_MAGIC || tmp.version != CONFIG_VERSION) return false;

  uint32_t expected = crc32Bytes(reinterpret_cast<const uint8_t *>(&tmp), offsetof(Config, crc));
  if (tmp.crc != expected) return false;

  cfg = tmp;
  cfg.staSsid[sizeof(cfg.staSsid) - 1] = '\0';
  cfg.staPass[sizeof(cfg.staPass) - 1] = '\0';
  cfg.apSsid[sizeof(cfg.apSsid) - 1] = '\0';
  cfg.apPass[sizeof(cfg.apPass) - 1] = '\0';
  cfg.adminPass[sizeof(cfg.adminPass) - 1] = '\0';
  return true;
}

// ----------------------------- Domain normalization / hash lists -----------------------------

static uint32_t fnv1a(const char *s) {
  uint32_t h = 2166136261UL;
  while (*s) {
    h ^= static_cast<uint8_t>(*s++);
    h *= 16777619UL;
  }
  return h ? h : 1UL;
}

static int compareU32(const void *a, const void *b) {
  uint32_t aa = *reinterpret_cast<const uint32_t *>(a);
  uint32_t bb = *reinterpret_cast<const uint32_t *>(b);
  if (aa < bb) return -1;
  if (aa > bb) return 1;
  return 0;
}

static uint16_t sortAndUnique(uint32_t *items, uint16_t count) {
  if (count < 2) return count;
  qsort(items, count, sizeof(uint32_t), compareU32);
  uint16_t w = 1;
  for (uint16_t r = 1; r < count; r++) {
    if (items[r] != items[w - 1]) items[w++] = items[r];
  }
  return w;
}

static bool hashExists(const uint32_t *items, uint16_t count, uint32_t target) {
  int lo = 0;
  int hi = static_cast<int>(count) - 1;
  while (lo <= hi) {
    int mid = lo + ((hi - lo) / 2);
    uint32_t v = items[mid];
    if (v == target) return true;
    if (v < target) lo = mid + 1;
    else hi = mid - 1;
  }
  return false;
}

static bool normalizeDomain(String input, char *out, size_t outSize) {
  if (!out || outSize < 4) return false;
  out[0] = '\0';

  input.trim();
  if (input.length() == 0) return false;

  // Strip inline comments.
  int hashPos = input.indexOf('#');
  if (hashPos >= 0) {
    input = input.substring(0, hashPos);
    input.trim();
    if (input.length() == 0) return false;
  }

  // Accept common Adblock syntax: ||example.com^
  if (input.startsWith("||")) input.remove(0, 2);
  if (input.endsWith("^")) input.remove(input.length() - 1);

  // Accept hosts-file syntax by selecting the final whitespace token.
  int lastSpace = -1;
  for (int i = 0; i < static_cast<int>(input.length()); i++) {
    if (input[i] == ' ' || input[i] == '\t') lastSpace = i;
  }
  if (lastSpace >= 0 && lastSpace + 1 < static_cast<int>(input.length())) {
    input = input.substring(lastSpace + 1);
    input.trim();
  }

  input.toLowerCase();

  if (input.startsWith("http://")) input.remove(0, 7);
  else if (input.startsWith("https://")) input.remove(0, 8);

  if (input.startsWith("*.")) input.remove(0, 2);

  int slash = input.indexOf('/');
  if (slash >= 0) input = input.substring(0, slash);

  // Strip :port for normal DNS names, but do not try to parse IPv6 literals.
  int colon = input.indexOf(':');
  if (colon >= 0) input = input.substring(0, colon);

  while (input.endsWith(".")) input.remove(input.length() - 1);
  while (input.startsWith(".")) input.remove(0, 1);

  if (input.length() < 1 || input.length() >= outSize || input.length() > 253) return false;

  bool onlyDigitsDots = true;
  bool lastWasDot = true;
  uint8_t labelLen = 0;

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];

    if (c == '.') {
      if (lastWasDot || labelLen == 0 || labelLen > 63) return false;
      lastWasDot = true;
      labelLen = 0;
      continue;
    }

    if (!((c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') ||
          c == '-' || c == '_')) {
      return false;
    }

    if (!(c >= '0' && c <= '9')) onlyDigitsDots = false;
    lastWasDot = false;
    labelLen++;
  }

  if (lastWasDot || labelLen == 0 || labelLen > 63) return false;
  if (onlyDigitsDots) return false; // Do not treat IP literals as block domains.

  safeCopy(out, outSize, input);
  return out[0] != '\0';
}

static bool domainMatchesHashes(const char *domain, const uint32_t *items, uint16_t count) {
  if (!domain || !*domain || count == 0) return false;

  const char *candidate = domain;
  while (candidate && *candidate) {
    if (hashExists(items, count, fnv1a(candidate))) return true;

    const char *dot = strchr(candidate, '.');
    if (!dot) break;
    candidate = dot + 1;

    // Never compare a bare top-level label such as "com".
    if (!strchr(candidate, '.')) break;
  }
  return false;
}

static bool isDomainBlocked(const char *domain) {
  if (!cfg.filterEnabled) return false;

  // Allowlist always wins, even over a blocked parent domain.
  if (domainMatchesHashes(domain, allowHashes, allowCount)) return false;
  return domainMatchesHashes(domain, blockHashes, blockCount);
}

static uint16_t loadHashFile(const char *path, uint32_t *dest, uint16_t maxItems, bool &truncated) {
  truncated = false;
  uint16_t count = 0;

  File f = LittleFS.open(path, "r");
  if (!f) return 0;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    char domain[128];

    if (!normalizeDomain(line, domain, sizeof(domain))) {
      yield();
      continue;
    }

    if (count >= maxItems) {
      truncated = true;
      break;
    }

    dest[count++] = fnv1a(domain);
    if ((count & 0x1F) == 0) yield();
  }

  f.close();
  return sortAndUnique(dest, count);
}

static void reloadLists() {
  blockCount = loadHashFile(BLOCK_FILE, blockHashes, MAX_BLOCK_HASHES, blockListTruncated);
  allowCount = loadHashFile(ALLOW_FILE, allowHashes, MAX_ALLOW_HASHES, allowListTruncated);
}

static void seedFilesIfNeeded() {
  if (!LittleFS.exists(BLOCK_FILE)) {
    File f = LittleFS.open(BLOCK_FILE, "w");
    if (f) {
      f.print(F(
        "# ESPHole starter blocklist\n"
        "# One domain per line. Hosts-file and ||domain^ forms are also accepted.\n"
        "doubleclick.net\n"
        "googlesyndication.com\n"
        "googleadservices.com\n"
        "adservice.google.com\n"
        "adnxs.com\n"
        "adsrvr.org\n"
        "scorecardresearch.com\n"
        "taboola.com\n"
        "outbrain.com\n"
        "amazon-adsystem.com\n"
        "criteo.com\n"
        "criteo.net\n"
        "pubmatic.com\n"
        "rubiconproject.com\n"
        "openx.net\n"
        "casalemedia.com\n"
        "quantserve.com\n"
        "moatads.com\n"
        "smartadserver.com\n"
        "media.net\n"
      ));
      f.close();
    }
  }

  if (!LittleFS.exists(ALLOW_FILE)) {
    File f = LittleFS.open(ALLOW_FILE, "w");
    if (f) {
      f.print(F("# ESPHole allowlist - one domain per line\n"));
      f.close();
    }
  }
}

static uint16_t appendDomainBatch(const char *path, const String &batch, bool isAllowList) {
  File f = LittleFS.open(path, "a");
  if (!f) return 0;

  uint16_t added = 0;
  int start = 0;

  while (start <= static_cast<int>(batch.length())) {
    int end = batch.indexOf('\n', start);
    if (end < 0) end = batch.length();

    String line = batch.substring(start, end);
    char domain[128];

    if (normalizeDomain(line, domain, sizeof(domain))) {
      uint32_t h = fnv1a(domain);
      bool exists = isAllowList
        ? hashExists(allowHashes, allowCount, h)
        : hashExists(blockHashes, blockCount, h);

      if (!exists) {
        f.println(domain);
        added++;
      }
    }

    start = end + 1;
    if ((added & 0x0F) == 0) yield();
    if (end >= static_cast<int>(batch.length())) break;
  }

  f.close();
  reloadLists();
  return added;
}

// ----------------------------- DNS parsing -----------------------------

static bool parseDnsQuestion(const uint8_t *packet, size_t len,
                             char *domain, size_t domainSize,
                             uint16_t &qtype, size_t &questionEnd) {
  if (!packet || len < 17 || !domain || domainSize < 2) return false;

  uint16_t qdCount = (static_cast<uint16_t>(packet[4]) << 8) | packet[5];
  if (qdCount != 1) return false;

  size_t pos = 12;
  size_t outPos = 0;
  bool first = true;

  while (pos < len) {
    uint8_t labelLen = packet[pos++];

    if (labelLen == 0) break;

    // Compression pointers should not normally appear in the question name.
    if ((labelLen & 0xC0) != 0 || labelLen > 63) return false;
    if (pos + labelLen > len) return false;

    if (!first) {
      if (outPos + 1 >= domainSize) return false;
      domain[outPos++] = '.';
    }
    first = false;

    for (uint8_t i = 0; i < labelLen; i++) {
      if (outPos + 1 >= domainSize) return false;
      char c = static_cast<char>(packet[pos++]);
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
      domain[outPos++] = c;
    }
  }

  if (first || pos + 4 > len) return false;

  domain[outPos] = '\0';
  qtype = (static_cast<uint16_t>(packet[pos]) << 8) | packet[pos + 1];
  uint16_t qclass = (static_cast<uint16_t>(packet[pos + 2]) << 8) | packet[pos + 3];
  questionEnd = pos + 4;

  return qclass == 1; // IN
}

static void sendDnsPacket(const uint8_t *data, size_t len, const IPAddress &ip, uint16_t port) {
  dnsClientUdp.beginPacket(ip, port);
  dnsClientUdp.write(data, len);
  dnsClientUdp.endPacket();
}

static void sendDnsError(const uint8_t *query, size_t len,
                         const IPAddress &clientIp, uint16_t clientPort,
                         uint8_t rcode) {
  if (len < 12 || len > DNS_MAX_PACKET) return;

  uint8_t resp[DNS_MAX_PACKET];
  memcpy(resp, query, len);

  // Preserve Opcode and RD. Set QR=1, RA=1, clear answer/authority/additional counts.
  resp[2] = static_cast<uint8_t>((resp[2] & 0x79) | 0x80);
  resp[3] = static_cast<uint8_t>(0x80 | (rcode & 0x0F));
  resp[6] = 0; resp[7] = 0;
  resp[8] = 0; resp[9] = 0;
  resp[10] = 0; resp[11] = 0;

  sendDnsPacket(resp, len, clientIp, clientPort);
}

static void sendCaptiveAResponse(const uint8_t *query, size_t queryLen, size_t questionEnd,
                                 const IPAddress &clientIp, uint16_t clientPort) {
  if (questionEnd + 16 > DNS_MAX_PACKET || questionEnd > queryLen) {
    sendDnsError(query, queryLen, clientIp, clientPort, 2);
    return;
  }

  uint8_t resp[DNS_MAX_PACKET];
  memcpy(resp, query, questionEnd);

  resp[2] = static_cast<uint8_t>((resp[2] & 0x79) | 0x80);
  resp[3] = 0x80; // NoError + recursion available
  resp[6] = 0; resp[7] = 1;  // 1 answer
  resp[8] = 0; resp[9] = 0;
  resp[10] = 0; resp[11] = 0;

  size_t p = questionEnd;

  // NAME: compression pointer to QNAME at offset 12.
  resp[p++] = 0xC0;
  resp[p++] = 0x0C;

  // TYPE A, CLASS IN
  resp[p++] = 0x00; resp[p++] = 0x01;
  resp[p++] = 0x00; resp[p++] = 0x01;

  // TTL = 0
  resp[p++] = 0x00; resp[p++] = 0x00; resp[p++] = 0x00; resp[p++] = 0x00;

  // RDLENGTH = 4
  resp[p++] = 0x00; resp[p++] = 0x04;

  resp[p++] = AP_IP[0];
  resp[p++] = AP_IP[1];
  resp[p++] = AP_IP[2];
  resp[p++] = AP_IP[3];

  sendDnsPacket(resp, p, clientIp, clientPort);
}

static uint32_t cacheKeyFor(const char *domain, uint16_t qtype) {
  uint32_t h = fnv1a(domain);
  h ^= (static_cast<uint32_t>(qtype) * 0x9E3779B1UL);
  return h ? h : 1UL;
}

static bool cacheLookup(uint32_t key, uint16_t requestId,
                        const IPAddress &clientIp, uint16_t clientPort) {
  uint32_t now = millis();

  for (uint8_t i = 0; i < DNS_CACHE_SLOTS; i++) {
    DnsCacheEntry &e = dnsCache[i];
    if (!e.valid || e.key != key) continue;

    if (static_cast<int32_t>(now - e.expiresAt) >= 0) {
      e.valid = 0;
      continue;
    }

    if (e.len < 12 || e.len > DNS_MAX_PACKET) {
      e.valid = 0;
      continue;
    }

    uint8_t resp[DNS_MAX_PACKET];
    memcpy(resp, e.data, e.len);
    resp[0] = static_cast<uint8_t>(requestId >> 8);
    resp[1] = static_cast<uint8_t>(requestId & 0xFF);

    sendDnsPacket(resp, e.len, clientIp, clientPort);
    cacheHits++;
    return true;
  }

  return false;
}

static void cacheStore(uint32_t key, const uint8_t *data, uint16_t len) {
  if (!data || len < 12 || len > DNS_MAX_PACKET) return;

  DnsCacheEntry &e = dnsCache[cacheReplaceIndex++ % DNS_CACHE_SLOTS];
  e.key = key;
  e.len = len;
  e.expiresAt = millis() + DNS_CACHE_MS;
  memcpy(e.data, data, len);
  e.valid = 1;
}

static void recordRecent(const char *domain, uint16_t qtype, bool blocked) {
  RecentQuery &r = recentQueries[recentHead];
  safeCopyC(r.domain, sizeof(r.domain), domain);
  r.qtype = qtype;
  r.blocked = blocked ? 1 : 0;
  r.whenMs = millis();

  recentHead = static_cast<uint8_t>((recentHead + 1) % RECENT_QUERY_COUNT);
  if (recentUsed < RECENT_QUERY_COUNT) recentUsed++;
}

static void drainUpstreamDnsSocket() {
  while (true) {
    int n = dnsUpstreamUdp.parsePacket();
    if (n <= 0) break;
    while (dnsUpstreamUdp.available()) dnsUpstreamUdp.read();
    yield();
  }
}

static bool forwardDnsQuery(const uint8_t *query, uint16_t queryLen,
                            uint16_t requestId, uint32_t cacheKey,
                            const IPAddress &clientIp, uint16_t clientPort) {
  IPAddress upstream = configuredUpstreamDns();
  if (ipIsZero(upstream)) return false;

  drainUpstreamDnsSocket();

  if (!dnsUpstreamUdp.beginPacket(upstream, DNS_PORT)) return false;
  dnsUpstreamUdp.write(query, queryLen);
  if (!dnsUpstreamUdp.endPacket()) return false;

  uint32_t started = millis();
  while (static_cast<uint32_t>(millis() - started) < DNS_TIMEOUT_MS) {
    int n = dnsUpstreamUdp.parsePacket();
    if (n > 0) {
      if (n > DNS_MAX_PACKET) {
        while (dnsUpstreamUdp.available()) dnsUpstreamUdp.read();
        continue;
      }

      uint8_t resp[DNS_MAX_PACKET];
      int got = dnsUpstreamUdp.read(resp, sizeof(resp));
      if (got < 12) continue;

      uint16_t responseId = (static_cast<uint16_t>(resp[0]) << 8) | resp[1];
      if (responseId != requestId) continue;

      sendDnsPacket(resp, got, clientIp, clientPort);
      cacheStore(cacheKey, resp, static_cast<uint16_t>(got));
      return true;
    }

    delay(1);
  }

  return false;
}

static void processDnsOnce() {
  int packetSize = dnsClientUdp.parsePacket();
  if (packetSize <= 0) return;

  IPAddress clientIp = dnsClientUdp.remoteIP();
  uint16_t clientPort = dnsClientUdp.remotePort();

  if (packetSize > DNS_MAX_PACKET) {
    while (dnsClientUdp.available()) dnsClientUdp.read();
    return;
  }

  uint8_t query[DNS_MAX_PACKET];
  int got = dnsClientUdp.read(query, sizeof(query));
  if (got < 12) return;

  char domain[128];
  uint16_t qtype = 0;
  size_t questionEnd = 0;

  if (!parseDnsQuestion(query, got, domain, sizeof(domain), qtype, questionEnd)) {
    sendDnsError(query, got, clientIp, clientPort, 1); // FORMERR
    return;
  }

  // When upstream is unavailable, behave as a recovery captive portal.
  if (captiveMode()) {
    if (qtype == 1) {
      sendCaptiveAResponse(query, got, questionEnd, clientIp, clientPort);
    } else {
      // NOERROR, no answers. This avoids sending malformed A data to AAAA/etc.
      sendDnsError(query, questionEnd, clientIp, clientPort, 0);
    }
    return;
  }

  totalQueries++;

  if (isDomainBlocked(domain)) {
    blockedQueries++;
    recordRecent(domain, qtype, true);

    // Pi-hole supports several blocking modes. ESPHole uses NXDOMAIN because
    // it is compact and type-independent.
    sendDnsError(query, questionEnd, clientIp, clientPort, 3);
    return;
  }

  recordRecent(domain, qtype, false);

  uint16_t requestId = (static_cast<uint16_t>(query[0]) << 8) | query[1];
  uint32_t key = cacheKeyFor(domain, qtype);

  if (cacheLookup(key, requestId, clientIp, clientPort)) return;

  forwardedQueries++;
  if (!forwardDnsQuery(query, static_cast<uint16_t>(got), requestId, key, clientIp, clientPort)) {
    upstreamFailures++;
    sendDnsError(query, questionEnd, clientIp, clientPort, 2); // SERVFAIL
  }
}

// ----------------------------- NAPT / repeater -----------------------------

static void enableNaptIfPossible() {
#if ESPHOLE_HAS_NAPT
  if (naptReady || !stationOnline()) return;

  if (!naptInitAttempted) {
    naptInitAttempted = true;
    err_t ret = ip_napt_init(NAPT_ENTRIES, NAPT_PORTMAP_ENTRIES);
    if (ret != ERR_OK) {
      Serial.printf("[NAPT] ip_napt_init failed: %d\n", static_cast<int>(ret));
      return;
    }
  }

  err_t ret = ip_napt_enable_no(SOFTAP_IF, 1);
  if (ret == ERR_OK) {
    naptReady = true;
    Serial.println(F("[NAPT] SoftAP NAT enabled"));
  } else {
    Serial.printf("[NAPT] enable failed: %d\n", static_cast<int>(ret));
  }
#else
  naptInitAttempted = true;
  naptReady = false;
#endif
}

// ----------------------------- HTTP UI -----------------------------

static bool requireAdmin() {
  // First-boot recovery portal is intentionally open so the owner can configure it.
  if (!cfg.configured) return true;

  if (server.authenticate("admin", cfg.adminPass)) return true;
  server.requestAuthentication(BASIC_AUTH, "ESPHole");
  return false;
}

static void redirectToRoot() {
  server.sendHeader(F("Location"), F("/"), true);
  server.send(303, F("text/plain"), F(""));
}

static void sendPageStart(const __FlashStringHelper *title) {
  server.chunkedResponseModeStart(200, F("text/html"));
  server.sendContent(F(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>"
    "body{font-family:system-ui,-apple-system,sans-serif;background:#101417;color:#e9eef2;margin:0}"
    "main{max-width:900px;margin:auto;padding:18px}"
    "h1,h2{margin:.4em 0}.card{background:#182026;border:1px solid #29343c;border-radius:14px;padding:15px;margin:12px 0}"
    "a{color:#70c7ff}code{background:#0d1114;padding:2px 6px;border-radius:6px}"
    "input,textarea,button{box-sizing:border-box;width:100%;padding:10px;margin:5px 0;border-radius:8px;border:1px solid #41505b;background:#0f1519;color:#fff}"
    "button{background:#1d6d9f;border:0;font-weight:700;cursor:pointer}"
    "button.danger{background:#922}.row{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:10px}"
    ".stat{font-size:1.6rem;font-weight:800}.muted{color:#9db0bc;font-size:.9rem}"
    "table{width:100%;border-collapse:collapse}td,th{padding:7px;border-bottom:1px solid #2b363e;text-align:left;font-size:.9rem}"
    ".ok{color:#68e08b}.bad{color:#ff7777}.pill{display:inline-block;padding:3px 8px;border-radius:999px;background:#26343d}"
    "</style><title>"
  ));
  server.sendContent(title);
  server.sendContent(F("</title></head><body><main>"));
}

static void sendPageEnd() {
  server.sendContent(F(
    "<p class='muted'>ESPHole v1.0.0 &middot; ESP8266 DNS sinkhole + NAPT repeater</p>"
    "</main></body></html>"
  ));
  server.chunkedResponseFinalize();
}

static void sendDashboard() {
  sendPageStart(F("ESPHole"));

  server.sendContent(F("<h1>ESPHole</h1><div class='card'><div class='row'>"));

  String s;
  s.reserve(512);

  s = F("<div><div class='muted'>Upstream Wi-Fi</div><div class='stat ");
  s += stationOnline() ? F("ok'>ONLINE") : F("bad'>OFFLINE");
  s += F("</div><div>");
  s += stationOnline() ? htmlEscape(WiFi.SSID()) : F("Captive setup mode");
  s += F("</div></div>");
  server.sendContent(s);

  s = F("<div><div class='muted'>Repeater AP</div><div class='stat'>");
  s += htmlEscape(String(cfg.apSsid));
  s += F("</div><div>");
  s += WiFi.softAPIP().toString();
  s += F(" &middot; ");
  s += String(WiFi.softAPgetStationNum());
  s += F(" client(s)</div></div>");
  server.sendContent(s);

  s = F("<div><div class='muted'>NAPT</div><div class='stat ");
  s += naptReady ? F("ok'>READY") : F("bad'>");
#if ESPHOLE_HAS_NAPT
  s += naptReady ? F("READY") : F("WAITING");
#else
  s += F("UNAVAILABLE");
#endif
  s += F("</div></div>");
  server.sendContent(s);

  s = F("<div><div class='muted'>Filtering</div><div class='stat ");
  s += cfg.filterEnabled ? F("ok'>ON") : F("bad'>OFF");
  s += F("</div><div>");
  s += String(blockCount);
  s += F(" block / ");
  s += String(allowCount);
  s += F(" allow</div></div>");
  server.sendContent(s);

  server.sendContent(F("</div></div>"));

  server.sendContent(F("<div class='card'><h2>DNS statistics</h2><div class='row'>"));

  s = F("<div><div class='muted'>Queries</div><div class='stat'>");
  s += String(totalQueries);
  s += F("</div></div><div><div class='muted'>Blocked</div><div class='stat'>");
  s += String(blockedQueries);
  s += F("</div></div><div><div class='muted'>Cache hits</div><div class='stat'>");
  s += String(cacheHits);
  s += F("</div></div><div><div class='muted'>Upstream failures</div><div class='stat'>");
  s += String(upstreamFailures);
  s += F("</div></div>");
  server.sendContent(s);

  server.sendContent(F("</div></div>"));

  server.sendContent(F("<div class='card'><h2>Recent DNS</h2><table><tr><th>Domain</th><th>Type</th><th>Result</th></tr>"));
  if (recentUsed == 0) {
    server.sendContent(F("<tr><td colspan='3' class='muted'>No queries yet.</td></tr>"));
  } else {
    for (uint8_t i = 0; i < recentUsed; i++) {
      int idx = static_cast<int>(recentHead) - 1 - i;
      while (idx < 0) idx += RECENT_QUERY_COUNT;
      const RecentQuery &r = recentQueries[idx];

      s = F("<tr><td>");
      s += htmlEscape(String(r.domain));
      s += F("</td><td>");
      s += qtypeName(r.qtype);
      s += F("</td><td>");
      s += r.blocked ? F("<span class='bad'>BLOCKED</span>") : F("<span class='ok'>ALLOWED</span>");
      s += F("</td></tr>");
      server.sendContent(s);
    }
  }
  server.sendContent(F("</table></div>"));

  if (blockListTruncated || allowListTruncated) {
    server.sendContent(F("<div class='card bad'><b>List limit reached.</b> Some entries were not loaded into RAM. "
                         "Reduce the list or increase the compile-time limits if your board has enough heap.</div>"));
  }

  server.sendContent(F(
    "<div class='card'><h2>Management</h2>"
    "<p><a href='/admin'>Settings &amp; lists</a> &middot; "
    "<a href='/scan'>Scan Wi-Fi</a></p>"
    "<p class='muted'>Admin username is <code>admin</code>. On first boot the setup page is open; after configuration, changes require the admin password.</p>"
    "</div>"
  ));

  sendPageEnd();
}

static void sendAdminPage() {
  if (!requireAdmin()) return;

  sendPageStart(F("ESPHole Admin"));

  String s;

  server.sendContent(F("<h1>ESPHole Admin</h1><p><a href='/'>&larr; Dashboard</a></p>"));

  server.sendContent(F("<div class='card'><h2>Upstream Wi-Fi</h2>"
                       "<form method='post' action='/savewifi'>"
                       "<label>SSID</label><input name='ssid' maxlength='32' value='"));
  server.sendContent(htmlEscape(String(cfg.staSsid)));
  server.sendContent(F("'><label>Password</label>"
                       "<input name='pass' type='password' maxlength='64' placeholder='Leave blank to keep current; type OPEN to clear'>"
                       "<label>Custom upstream DNS (blank = DHCP/automatic)</label><input name='dns' placeholder='1.1.1.1' value='"));

  if (cfg.useCustomDns) {
    IPAddress d(cfg.customDns[0], cfg.customDns[1], cfg.customDns[2], cfg.customDns[3]);
    server.sendContent(d.toString());
  }

  server.sendContent(F("'><button>Save Wi-Fi &amp; reboot</button></form></div>"));

  server.sendContent(F("<div class='card'><h2>Repeater / admin</h2>"
                       "<form method='post' action='/saveap'>"
                       "<label>AP SSID</label><input name='apssid' maxlength='32' value='"));
  server.sendContent(htmlEscape(String(cfg.apSsid)));
  server.sendContent(F("'><label>AP password</label>"
                       "<input name='appass' type='password' maxlength='64' placeholder='Leave blank to keep current; type OPEN for open AP'>"
                       "<label>Admin password</label>"
                       "<input name='adminpass' type='password' maxlength='32' placeholder='Leave blank to keep current'>"
                       "<label><input style='width:auto' type='checkbox' name='filter' value='1' "));
  if (cfg.filterEnabled) server.sendContent(F("checked"));
  server.sendContent(F("> Enable DNS filtering</label><button>Save settings &amp; reboot</button></form></div>"));

  server.sendContent(F("<div class='card'><h2>Blocklist</h2><p>Loaded: "));
  s = String(blockCount);
  s += F(" / max ");
  s += String(MAX_BLOCK_HASHES);
  s += F("</p>");
  server.sendContent(s);
  server.sendContent(F("<form method='post' action='/block/add'><textarea name='domains' rows='7' "
                       "placeholder='ads.example.com&#10;0.0.0.0 tracker.example.net&#10;||telemetry.example.org^'></textarea>"
                       "<button>Add domains</button></form>"
                       "<p><a href='/blocklist.txt'>Download current blocklist</a></p>"
                       "<form method='post' action='/block/clear' onsubmit=\"return confirm('Clear blocklist?')\">"
                       "<button class='danger'>Clear blocklist</button></form></div>"));

  server.sendContent(F("<div class='card'><h2>Allowlist</h2><p>Loaded: "));
  s = String(allowCount);
  s += F(" / max ");
  s += String(MAX_ALLOW_HASHES);
  s += F("</p>");
  server.sendContent(s);
  server.sendContent(F("<form method='post' action='/allow/add'><textarea name='domains' rows='5' "
                       "placeholder='needed.example.com'></textarea><button>Add domains</button></form>"
                       "<p><a href='/allowlist.txt'>Download current allowlist</a></p>"
                       "<form method='post' action='/allow/clear' onsubmit=\"return confirm('Clear allowlist?')\">"
                       "<button class='danger'>Clear allowlist</button></form></div>"));

  server.sendContent(F("<div class='card'><h2>Maintenance</h2>"
                       "<form method='post' action='/reboot'><button>Reboot ESPHole</button></form>"
                       "<form method='post' action='/factory' onsubmit=\"return confirm('Factory reset ESPHole and lists?')\">"
                       "<button class='danger'>Factory reset</button></form></div>"));

  sendPageEnd();
}

static void handleScan() {
  if (!requireAdmin()) return;

  sendPageStart(F("Wi-Fi Scan"));
  server.sendContent(F("<h1>Wi-Fi scan</h1><p><a href='/admin'>&larr; Admin</a></p><div class='card'>"));

  int n = WiFi.scanNetworks(false, true);
  if (n <= 0) {
    server.sendContent(F("<p>No networks found.</p>"));
  } else {
    server.sendContent(F("<table><tr><th>SSID</th><th>RSSI</th><th>Security</th><th>Channel</th></tr>"));
    for (int i = 0; i < n; i++) {
      String row = F("<tr><td>");
      row += htmlEscape(WiFi.SSID(i));
      row += F("</td><td>");
      row += String(WiFi.RSSI(i));
      row += F(" dBm</td><td>");
      row += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? F("Open") : F("Protected");
      row += F("</td><td>");
      row += String(WiFi.channel(i));
      row += F("</td></tr>");
      server.sendContent(row);
      yield();
    }
    server.sendContent(F("</table>"));
  }

  WiFi.scanDelete();
  server.sendContent(F("</div>"));
  sendPageEnd();
}

static void handleSaveWiFi() {
  if (!requireAdmin()) return;

  String ssid = server.arg("ssid");
  ssid.trim();

  if (ssid.length() == 0 || ssid.length() > 32) {
    server.send(400, F("text/plain"), F("SSID is required and must be <= 32 characters."));
    return;
  }

  safeCopy(cfg.staSsid, sizeof(cfg.staSsid), ssid);

  String pass = server.arg("pass");
  if (pass == "OPEN") {
    cfg.staPass[0] = '\0';
  } else if (pass.length() > 0) {
    safeCopy(cfg.staPass, sizeof(cfg.staPass), pass);
  }

  String dns = server.arg("dns");
  dns.trim();

  if (dns.length() == 0) {
    cfg.useCustomDns = 0;
  } else {
    IPAddress parsed;
    if (!parsed.fromString(dns)) {
      server.send(400, F("text/plain"), F("Invalid DNS IPv4 address."));
      return;
    }
    cfg.useCustomDns = 1;
    for (uint8_t i = 0; i < 4; i++) cfg.customDns[i] = parsed[i];
  }

  cfg.configured = 1;
  if (!saveConfig()) {
    server.send(500, F("text/plain"), F("Could not save configuration."));
    return;
  }

  server.send(200, F("text/html"), F("<html><body><h2>Saved.</h2><p>ESPHole is rebooting...</p></body></html>"));
  pendingRestartAt = millis() + 1200UL;
}

static void handleSaveAp() {
  if (!requireAdmin()) return;

  String apSsid = server.arg("apssid");
  apSsid.trim();
  if (apSsid.length() == 0 || apSsid.length() > 32) {
    server.send(400, F("text/plain"), F("AP SSID must be 1-32 characters."));
    return;
  }
  safeCopy(cfg.apSsid, sizeof(cfg.apSsid), apSsid);

  String apPass = server.arg("appass");
  if (apPass == "OPEN") {
    cfg.apPass[0] = '\0';
  } else if (apPass.length() > 0) {
    if (apPass.length() < 8 || apPass.length() > 63) {
      server.send(400, F("text/plain"), F("AP password must be 8-63 characters, or OPEN."));
      return;
    }
    safeCopy(cfg.apPass, sizeof(cfg.apPass), apPass);
  }

  String adminPass = server.arg("adminpass");
  if (adminPass.length() > 0) {
    if (adminPass.length() < 6 || adminPass.length() > 32) {
      server.send(400, F("text/plain"), F("Admin password must be 6-32 characters."));
      return;
    }
    safeCopy(cfg.adminPass, sizeof(cfg.adminPass), adminPass);
  }

  cfg.filterEnabled = server.hasArg("filter") ? 1 : 0;

  if (!saveConfig()) {
    server.send(500, F("text/plain"), F("Could not save configuration."));
    return;
  }

  server.send(200, F("text/html"), F("<html><body><h2>Saved.</h2><p>ESPHole is rebooting...</p></body></html>"));
  pendingRestartAt = millis() + 1200UL;
}

static void handleBlockAdd() {
  if (!requireAdmin()) return;
  uint16_t added = appendDomainBatch(BLOCK_FILE, server.arg("domains"), false);
  server.sendHeader(F("Location"), F("/admin"), true);
  server.sendHeader(F("X-ESPHole-Added"), String(added));
  server.send(303, F("text/plain"), F(""));
}

static void handleAllowAdd() {
  if (!requireAdmin()) return;
  uint16_t added = appendDomainBatch(ALLOW_FILE, server.arg("domains"), true);
  server.sendHeader(F("Location"), F("/admin"), true);
  server.sendHeader(F("X-ESPHole-Added"), String(added));
  server.send(303, F("text/plain"), F(""));
}

static void handleClearList(const char *path, const __FlashStringHelper *header) {
  if (!requireAdmin()) return;
  File f = LittleFS.open(path, "w");
  if (f) {
    f.println(header);
    f.close();
  }
  reloadLists();
  redirectToRoot();
}

static void handleDownloadFile(const char *path) {
  if (!requireAdmin()) return;
  File f = LittleFS.open(path, "r");
  if (!f) {
    server.send(404, F("text/plain"), F("File not found"));
    return;
  }
  server.streamFile(f, F("text/plain"));
  f.close();
}

static void handleReboot() {
  if (!requireAdmin()) return;
  server.send(200, F("text/plain"), F("Rebooting ESPHole..."));
  pendingRestartAt = millis() + 700UL;
}

static void handleFactoryReset() {
  if (!requireAdmin()) return;

  LittleFS.remove(CONFIG_FILE);
  LittleFS.remove(BLOCK_FILE);
  LittleFS.remove(ALLOW_FILE);

  server.send(200, F("text/plain"), F("Factory reset complete. Rebooting..."));
  pendingRestartAt = millis() + 900UL;
}

static void handleConnectivityCheck() {
  if (captiveMode()) {
    server.sendHeader(F("Location"), F("http://10.42.0.1/"), true);
    server.send(302, F("text/plain"), F(""));
    return;
  }

  // Online: do not keep devices trapped in captive-portal state.
  String uri = server.uri();
  if (uri.startsWith("/generate_204")) {
    server.send(204, F("text/plain"), F(""));
  } else if (uri == "/hotspot-detect.html") {
    server.send(200, F("text/html"), F("<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"));
  } else if (uri == "/ncsi.txt") {
    server.send(200, F("text/plain"), F("Microsoft NCSI"));
  } else if (uri == "/connecttest.txt") {
    server.send(200, F("text/plain"), F("Microsoft Connect Test"));
  } else {
    server.send(204, F("text/plain"), F(""));
  }
}

static void handleNotFound() {
  String uri = server.uri();

  // Android 11+ may use generated variants such as /generate_204_<uuid>.
  // Treat those like the registered connectivity-check endpoints.
  if (uri.startsWith("/generate_204") ||
      uri.startsWith("/gen_204") ||
      uri.startsWith("/hotspot-detect") ||
      uri.startsWith("/library/test/success")) {
    handleConnectivityCheck();
    return;
  }

  if (captiveMode()) {
    server.sendHeader(F("Location"), F("http://10.42.0.1/"), true);
    server.send(302, F("text/plain"), F(""));
  } else {
    server.send(404, F("text/plain"), F("Not found"));
  }
}

static void setupHttpServer() {
  server.on("/", HTTP_GET, sendDashboard);
  server.on("/admin", HTTP_GET, sendAdminPage);
  server.on("/scan", HTTP_GET, handleScan);

  server.on("/savewifi", HTTP_POST, handleSaveWiFi);
  server.on("/saveap", HTTP_POST, handleSaveAp);
  server.on("/block/add", HTTP_POST, handleBlockAdd);
  server.on("/allow/add", HTTP_POST, handleAllowAdd);

  server.on("/block/clear", HTTP_POST, []() {
    handleClearList(BLOCK_FILE, F("# ESPHole blocklist\n"));
  });
  server.on("/allow/clear", HTTP_POST, []() {
    handleClearList(ALLOW_FILE, F("# ESPHole allowlist\n"));
  });

  server.on("/blocklist.txt", HTTP_GET, []() { handleDownloadFile(BLOCK_FILE); });
  server.on("/allowlist.txt", HTTP_GET, []() { handleDownloadFile(ALLOW_FILE); });

  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/factory", HTTP_POST, handleFactoryReset);

  // Common captive portal probes.
  server.on("/generate_204", HTTP_ANY, handleConnectivityCheck);
  server.on("/gen_204", HTTP_ANY, handleConnectivityCheck);
  server.on("/hotspot-detect.html", HTTP_ANY, handleConnectivityCheck);
  server.on("/library/test/success.html", HTTP_ANY, handleConnectivityCheck);
  server.on("/ncsi.txt", HTTP_ANY, handleConnectivityCheck);
  server.on("/connecttest.txt", HTTP_ANY, handleConnectivityCheck);
  server.on("/fwlink", HTTP_ANY, handleConnectivityCheck);

  server.onNotFound(handleNotFound);
  server.begin();
}

// ----------------------------- Wi-Fi lifecycle -----------------------------

static void startSoftAp() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.persistent(false);

  // Make SoftAP clients use ESPHole itself for DNS.
  auto &dhcp = WiFi.softAPDhcpServer();
  dhcp.setDns(AP_IP);

  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);

  bool ok;
  if (strlen(cfg.apPass) == 0) {
    ok = WiFi.softAP(cfg.apSsid, nullptr, 1, 0, 8);
  } else {
    ok = WiFi.softAP(cfg.apSsid, cfg.apPass, 1, 0, 8);
  }

  Serial.printf("[AP] %s - %s - IP %s\n",
                ok ? "started" : "FAILED",
                cfg.apSsid,
                WiFi.softAPIP().toString().c_str());
}

static void beginStation() {
  if (!cfg.configured || strlen(cfg.staSsid) == 0) {
    Serial.println(F("[STA] No upstream Wi-Fi configured; captive setup mode"));
    return;
  }

  WiFi.hostname("ESPHole");
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.staSsid, cfg.staPass);

  Serial.printf("[STA] Connecting to %s\n", cfg.staSsid);
}

static void serviceWiFiState() {
  wl_status_t st = WiFi.status();
  if (st == lastStaStatus) {
    if (st == WL_CONNECTED) enableNaptIfPossible();
    return;
  }

  lastStaStatus = st;

  if (st == WL_CONNECTED) {
    Serial.printf("[STA] Connected. IP=%s GW=%s DNS=%s RSSI=%d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  configuredUpstreamDns().toString().c_str(),
                  WiFi.RSSI());
    enableNaptIfPossible();
  } else {
    Serial.printf("[STA] Status changed: %d\n", static_cast<int>(st));
  }
}

// ----------------------------- Arduino entry points -----------------------------

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("================================================"));
  Serial.printf(" ESPHole %s\n", ESPHOLE_VERSION);
  Serial.println(F(" DNS sinkhole + ESP8266 NAPT repeater"));
  Serial.println(F("================================================"));

  if (!LittleFS.begin()) {
    Serial.println(F("[FS] LittleFS mount failed; attempting one format"));
    if (!LittleFS.format() || !LittleFS.begin()) {
      Serial.println(F("[FS] LittleFS unavailable. Restarting in 5 seconds."));
      delay(5000);
      ESP.restart();
    }
  }

  bool firstBootCredentials = false;
  if (!loadConfig()) {
    Serial.println(F("[CFG] No valid config; creating first-boot configuration"));
    setDefaultConfig();
    if (!saveConfig()) {
      Serial.println(F("[CFG] WARNING: could not persist first-boot configuration"));
    }
    firstBootCredentials = true;
  }

  seedFilesIfNeeded();
  reloadLists();

  Serial.printf("[LIST] block=%u%s allow=%u%s\n",
                blockCount, blockListTruncated ? " (TRUNCATED)" : "",
                allowCount, allowListTruncated ? " (TRUNCATED)" : "");

  startSoftAp();

  if (!dnsClientUdp.begin(DNS_PORT)) {
    Serial.println(F("[DNS] Could not bind port 53"));
  } else {
    Serial.println(F("[DNS] Listening on UDP/53"));
  }

  if (!dnsUpstreamUdp.begin(DNS_UPSTREAM_LOCAL_PORT)) {
    Serial.println(F("[DNS] Could not bind upstream UDP socket"));
  }

  setupHttpServer();
  beginStation();

  Serial.print(F("[HTTP] Dashboard: http://"));
  Serial.print(AP_IP);
  Serial.println(F("/"));
  if (firstBootCredentials) {
    Serial.println(F("[FIRST BOOT] Save these credentials, then change them in Settings:"));
    Serial.printf("[FIRST BOOT] AP SSID: %s\n", cfg.apSsid);
    Serial.printf("[FIRST BOOT] AP password: %s\n", cfg.apPass);
    Serial.println(F("[FIRST BOOT] Admin username: admin"));
    Serial.printf("[FIRST BOOT] Admin password: %s\n", cfg.adminPass);
  }
}

void loop() {
  server.handleClient();
  processDnsOnce();
  serviceWiFiState();

  if (pendingRestartAt && static_cast<int32_t>(millis() - pendingRestartAt) >= 0) {
    delay(50);
    ESP.restart();
  }

  yield();
}