# Blaze Pisonet Universal

**Type:** ESP8266 multi-unit timer/controller firmware  
**Status:** Active engineering build

A configurable firmware architecture for standalone or centralized coin-operated computer timers.

## Architecture

The public source demonstrates:

- standalone, master, and slave roles;
- access-point or station networking;
- fallback access-point behavior;
- local node discovery and heartbeat messages;
- session request/credit workflow;
- relay/timer control;
- persistent configuration and sales counters;
- TM1637 display support;
- original generic buzzer notifications.

## Public Source Policy

The public edition contains no personal phone number, private Wi-Fi credential, customer configuration, private deployment address, or audio pattern derived from uploaded recordings.

## Preview

Open `preview.html` for a browser-only interface demonstration. It does not connect to or control an ESP8266.

## Validation Required

Before a production deployment, compile against the intended ESP8266 Arduino core and libraries, then test boot-pin safety, relay polarity, coin-input filtering, power-loss persistence, multi-node recovery, timer accuracy, and long-duration network behavior.
