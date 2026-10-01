# Blaze Pisonet Timer — Public-Safe Continuation

**Type:** ESP8266 timer / relay controller  
**Status:** SOURCE READY / HARDWARE VALIDATION REQUIRED

This is a publication-safe continuation of an earlier private Pisonet timer build. The private edition used embedded MP3 assets and a reusable default Wi-Fi password; those elements are intentionally not published.

## Purpose

The project demonstrates a compact ESP8266-based timer for coin-operated computer or appliance control. It runs as a local Wi-Fi access point with a browser configuration page and does not require internet access.

## Features

- configurable time added per coin pulse;
- two relay outputs;
- TM1637 four-digit countdown display;
- passive-buzzer coin, warning, and timeout tones;
- local AP + web settings interface;
- persistent remaining time and lifetime coin counter;
- generated per-device AP name and password on first boot;
- no cloud service or production endpoint.

## Default Pin Map

| Function | ESP8266 GPIO | NodeMCU label |
|---|---:|---|
| Coin input | 0 | D3 |
| Relay 1 | 5 | D1 |
| Relay 2 | 4 | D2 |
| Passive buzzer | 3 | RX |
| TM1637 CLK | 12 | D6 |
| TM1637 DIO | 13 | D7 |

## Build

Required Arduino libraries:

- ESP8266 core (ESP8266WiFi, ESP8266WebServer, EEPROM);
- TM1637Display.

Open `Blaze_Pisonet_Timer_PUBLIC.ino`, select the correct ESP8266 board, and compile/upload normally.

## Public-Safe Changes

This repository does **not** contain the private embedded-audio header or any MP3 payload. It also does not publish the historical universal AP password. The firmware generates credentials per device on first boot and lets the owner change them locally.

## Validation

The source has been structurally reviewed for the public portfolio, but this specific continuation has not been compiled against every ESP8266 core/library combination or tested on the final relay/coin/display hardware.

## Known Limitations

- powered-off elapsed time cannot be subtracted without an RTC;
- EEPROM persistence is event-based rather than transactional;
- GPIO3/RX is used as the default buzzer output, so serial/debug wiring may need adjustment;
- relay polarity and electrical isolation must be verified for the actual hardware.

## Project Status

**SOURCE READY / LAB**
