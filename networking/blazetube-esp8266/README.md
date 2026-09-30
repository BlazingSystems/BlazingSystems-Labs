# BlazeTube ESP8266

An ESP8266 Wi-Fi repeater and shared browser-session experiment built around the YouTube Data API and official embedded player.

## Purpose

BlazeTube explores whether a low-cost ESP8266 can coordinate a small shared video-browsing session across connected client browsers while also providing AP/STA routing through lwIP NAPT.

## Features

- ESP8266 SoftAP + upstream STA mode;
- lwIP NAPT/NAT for AP clients;
- up to three shared video/search tabs;
- one authoritative session synchronized across connected browsers;
- LittleFS session checkpoints for reboot recovery;
- cached search/home result metadata shared between clients;
- YouTube Data API v3 search integration using a user-supplied API key;
- official iframe playback in the client browser;
- local administration interface;
- unique first-boot AP/admin credentials generated and persisted automatically.

## Architecture / Technology

The ESP8266 coordinates session state and local web endpoints. Client browsers perform the actual video playback through the official embedded player. Search metadata uses the YouTube Data API v3.

No API key is bundled in the public source. The firmware does not proxy YouTube pages or attempt to bypass login, advertising, CORS, CSP, DRM or autoplay policy.

This project is not affiliated with or endorsed by Google or YouTube.

## Build / Usage

1. Use ESP8266 Arduino Core 3.1.2 with an lwIP v2 configuration that supports NAPT.
2. Compile and flash `BlazeTube_ESP8266_v1.1.0_PUBLIC.ino`.
3. Open Serial Monitor at 115200 on first boot and save the generated AP/admin credentials.
4. Configure upstream Wi-Fi and, if search is required, provide your own restricted YouTube Data API v3 key in Settings.
5. Keep the device on a trusted local network.

## Validation

- actual source artifact recovered;
- public source scanned for employer/client identifiers, private keys and embedded API credentials;
- universal public AP/admin passwords were replaced by generated, persisted first-boot credentials;
- edited source passed a delimiter/structure sanity check.

The public edition has **not** yet been compiled on the target ESP8266 core or validated with live NAPT/API/playback sessions.

## Known Limitations

- video rendering happens in the client browser, not on the ESP8266;
- internet/API availability is required for search and streaming;
- API quotas and service policies apply;
- browser autoplay behavior may require user interaction;
- ESP8266 RAM and Wi-Fi throughput constrain the number of clients and cached results;
- local administration uses HTTP and should not be exposed outside the trusted LAN.

## Future Work

- compile on NodeMCU/Wemos-class ESP8266 hardware;
- test multi-client state synchronization and reboot recovery;
- measure heap/throughput under simultaneous clients;
- validate API-key handling and quota failure behavior;
- test NAPT failure and upstream reconnection scenarios.

## Project Status

**LAB / SOURCE READY — compile, service-integration and hardware validation required.**
