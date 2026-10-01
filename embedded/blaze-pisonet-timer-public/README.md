# Blaze Pisonet Timer — Public-Safe Recovery

**Project Status:** LAB / RECOVERED DERIVATIVE

## Purpose

ESP8266 timer/controller firmware recovered from an earlier BlazingSystems build. The project combines coin-pulse timing, dual relay control, a TM1637 display, EEPROM persistence, local web configuration, sales counters, warning behavior, and optional audio playback.

## Public-Safe Changes

This directory is a publication-safe derivative of the recovered source, not a byte-for-byte archival copy.

- the original fixed default AP password was replaced with the placeholder `CHANGE_ME`;
- the original embedded MP3 payload is not redistributed;
- `embedded_audio_public.h` provides empty audio placeholders so the source structure remains buildable without bundled recordings;
- no private deployment values, customer data, employer data, or production credentials are included.

The unmodified recovered source and its original audio header remain outside public GitHub.

## Hardware / Dependencies

- ESP8266
- TM1637 display
- coin pulse input
- two relay outputs
- passive audio output on ESP8266 I2S NoDAC GPIO3/RX
- Arduino libraries: ESP8266 core, TM1637Display, ESP8266Audio

## Build / Usage

1. Open `Blaze_Pisonet_Timer_Public.ino` in the Arduino IDE.
2. Install the documented libraries.
3. Review GPIO assignments for your board.
4. Change the default AP password before deployment.
5. If audio is required, replace the empty arrays in `embedded_audio_public.h` only with recordings you created or are licensed to distribute.
6. Compile and test on the exact target hardware before connecting paid loads.

## Validation

The recovered implementation is structurally complete, but this public derivative has not been revalidated on target hardware after sanitization.

## Known Limitations

- bundled spoken/MP3 audio is intentionally disabled;
- sales day/month buckets in the recovered design are uptime-derived rather than calendar/RTC based;
- relay polarity and GPIO mapping must match the target cabinet;
- hardware-level electrical protection is outside the firmware.

## Future Work

- generated first-boot credentials;
- RTC/NTP-backed sales periods;
- hardware validation and brownout/recovery tests;
- replace embedded audio with user-owned recordings or synthesized tones.

