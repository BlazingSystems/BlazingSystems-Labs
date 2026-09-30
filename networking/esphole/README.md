# ESPHole

ESP8266 firmware combining a small DNS sinkhole, Wi-Fi NAT/repeater mode and a local management interface.

## Purpose

ESPHole explores whether an ESP8266 can provide lightweight network-wide DNS filtering for a small local network while also acting as the access point/NAT gateway.

It is inspired by the DNS-sinkhole role commonly associated with Pi-hole, but it is an independent implementation and does not contain the Pi-hole Linux codebase.

## Features

- ESP8266 SoftAP + upstream STA mode;
- lwIP NAPT/NAT routing for connected clients;
- local DNS server with domain block and allow lists;
- forwarding of allowed DNS queries to an upstream resolver;
- small response cache and runtime query statistics;
- LittleFS-backed settings and lists;
- captive setup mode when no upstream Wi-Fi is configured;
- local web dashboard;
- per-device first-boot AP/admin credentials generated and persisted automatically.

## Architecture / Technology

Target platform: ESP8266 Arduino Core 3.1.2 class boards such as NodeMCU, Wemos D1 mini and ESP-12E/F.

The firmware uses ESP8266 core libraries, LittleFS, UDP DNS handling and lwIP NAPT where supported. It is intentionally designed for the resource limits of the ESP8266.

## Build / Usage

1. Use an ESP8266 Arduino Core/lwIP build with NAPT support.
2. Compile and flash `ESPHole_PUBLIC.ino`.
3. Open Serial Monitor at 115200 on first boot and save the generated AP/admin credentials.
4. Connect to the ESPHole AP and complete upstream Wi-Fi setup.
5. Change credentials in Settings after initial setup.

## Validation

- actual source artifact recovered;
- public source scanned for employer/client identifiers, private keys and embedded production credentials;
- universal public default passwords were replaced with generated, persisted first-boot credentials;
- edited source passed a delimiter/structure sanity check.

An actual ESP8266 compile and hardware/NAPT/DNS test has **not** yet been completed for this public edition.

## Known Limitations

- not the Linux Pi-hole application;
- limited RAM constrains block/allow-list size and cache depth;
- encrypted DNS such as DoH/DoT/QUIC can bypass ordinary port-53 filtering;
- NAPT behavior depends on the selected ESP8266 core/lwIP configuration;
- web administration is local HTTP and should not be exposed to untrusted networks.

## Future Work

- compile against the documented ESP8266 core version;
- bench-test DHCP/DNS/NAPT behavior with multiple clients;
- measure heap use with large lists;
- test restart, filesystem corruption and factory-reset recovery;
- consider stronger local session/authentication controls.

## Project Status

**LAB / SOURCE READY — compile and hardware validation required.**
