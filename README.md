# BlazingSystems — Engineering Labs

Development-stage projects with coherent implementations that still require hardware, platform, integration, or release validation.

## Development Portfolio

| Project | Area | Stage | Project | Preview / interface |
|---|---|---|---|---|
| Blaze Pisonet Universal | ESP8266 / embedded networking | Hardware validation | [Open project](embedded/blaze-pisonet-universal/) | [Preview](embedded/blaze-pisonet-universal/preview.html) |
| Blaze Pisonet Timer — Public-Safe Recovery | ESP8266 / timer control | Recovered lab derivative | [Open project](embedded/blaze-pisonet-timer-public/) | [Preview](embedded/blaze-pisonet-timer-public/preview.html) |
| BlazeFM | Android utility | Build and device validation | [Open project](android/blazefm/) | Source project |
| MachDownload | Python / desktop networking | Working alpha | [Open project](software/machdownload/) | CLI / GUI source |
| Local AI Studio | Local Python engine | Engine prototype | [Open project](ai/local-ai-studio/) | [Preview](ai/local-ai-studio/preview.html) |
| OpenWrt VLAN Deployment Study | Networking | Configuration study | [Open project](networking/openwrt-vlan-deployment/) | Documentation |
| ESPHole | ESP8266 / DNS + NAPT | Source-ready lab | [Open project](networking/esphole/) | [Preview](networking/esphole/preview.html) |
| BlazeTube ESP8266 | ESP8266 / shared browser session + NAPT | Source-ready lab | [Open project](networking/blazetube-esp8266/) | [Preview](networking/blazetube-esp8266/preview.html) |

The repository landing page is [index.html](index.html).

## Lab Methodology

A Lab project should progress through:

1. source-structure review;
2. static/syntax validation;
3. controlled functional tests;
4. target hardware/platform testing;
5. failure and recovery testing;
6. documented limitations;
7. release-readiness review.

A project moves to the main portfolio only when its remaining validation requirements are sufficiently resolved.

## Publication Standard

Public Lab material excludes production credentials, private keys, raw router backups, employer/client records, private deployment data, and model weights that cannot be redistributed.

Examples use generic values, placeholders, or generated first-boot credentials. Hardware and networking projects must be adapted and independently validated for the target environment.
