# Safety / release gate

- [x] Separate old `v1.0-preview` artefacts remain unchanged
- [x] Native x64 and experimental native Windows ARM64 builds produced
- [x] DLL imports mapped to their Windows DLL, and resources embedded
- [x] Product information and author appear in executable resources
- [x] Approved owner-provided Logo V2 app icon cropped; no redraw
- [x] No telemetry, autorun or background service
- [x] Advanced Risk Mode is opt-in, 3-second visible countdown + final consent
- [x] Service whitelist only: WSearch and SysMain, with backup before write
- [x] Never forces running services to stop
- [x] No Defender/boot/pagefile/Windows Update manipulation
- [x] Safe cleanup still constrained to old top-level user temp files
- [ ] Actual Windows x64 runtime smoke test
- [ ] Windows ARM64 runtime smoke test
- [ ] Administrator-risk-mode rollback tested on same account and after reboot
- [ ] Hardware tests with 1 GB/16 GB, 2 GB/16 GB, old Pentium 4 GB, Chromebook/converted tablet
- [ ] Target-specific sustained CPU and disk-I/O counters / SSD SMART checks
- [ ] Authenticode signature from trusted code signing certificate

**Release classification: PREVIEW ONLY.**
