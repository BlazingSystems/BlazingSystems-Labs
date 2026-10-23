# Blaze Ultimate Optimizer v1.1 — Preview

**Blaze Systems — Published by Loumer Cabaluna Manreal**  
**Offline, native Win32, self-contained; no installer, no .NET, no PowerShell, no network calls.**

## Choose the right build

| Executable | Target | Status |
|---|---|---|
| `BlazeUltimateOptimizer-x64.exe` | 64-bit Intel/AMD Windows 10/11, including Windows-converted Intel Chromebooks and compatible tablets | Cross-compiled preview |
| `BlazeUltimateOptimizer-arm64.exe` | Windows on ARM64, only when the device actually boots Windows ARM64 and provides compatible system APIs | Cross-compiled **experimental** preview |

**These executables do NOT run on Linux or Android.** Orange Pi running Linux requires a separate native/Linux implementation. Chromebooks still on ChromeOS cannot run these without a Windows environment. Windows x86 (32-bit) is not included.

## Features

- **1-CLICK DIAGNOSE**: 1.5-second sampled CPU, physical/commit usage, OS drive capacity, uptime, Run-key startups, top working sets and aggregated per-app private committed bytes, classic Windows System/Application log indicators over seven days.
- **Crash details**: captures *candidate* application names and faulting modules from classic Application Error event ID 1000 insertion strings. Verify in Event Viewer; not all events include these strings, and they are not proof of cause.
- **1-CLICK SAFE FIX**: backs up and disables per-user UI animations and transparency, safely cleans only 7-day-old top-level files in `%LOCALAPPDATA%\Temp`, and rechecks. **It may show no measurable RAM reduction and never guarantees a speed boost.**
- **ADVANCED RISK MODE**: clear first warning, **visible 3-second countdown**, second confirmation, *whitelisted* Windows Search (`WSearch`) and SysMain (`SysMain`) startup changes only. Changes affect **startup** (service isn't forcibly terminated). Administrator permission is required; no UAC bypass.
- **RESTORE SERVICES**: reads persistent, per-user backup of each service's original start type and tries to restore it. Use the same elevated Windows account used to change the settings. A restart may be needed; service rollback is best-effort, not disaster recovery.
- **RESTORE PREFERENCES**: restores backed-up UI animation and transparency settings. Temp deletions cannot be undone.
- **REPORT**: UTF-16 offline report at `%LOCALAPPDATA%\BlazeUltimateOptimizer-Report.txt`.
- **BLAZE SYSTEMS branding**: deep-blue UI, blue controls and executable icon extracted **without redesign** from the approved identity board. Version information says `CompanyName: Loumer Cabaluna Manreal` and product is Blaze Ultimate Optimizer.

## Safety boundaries

Advanced Risk Mode doesn't switch off arbitrary Windows services; it has an allowlist of `WSearch` and `SysMain`. Even these may worsen system performance or interrupt Search/Indexing, so **use only when supported by observed diagnostics**. Do not disable Windows Defender, Windows Update, Cryptographic Services, network stack, login/boot services, pagefile, or storage drivers. Some Windows editions restrict changes to these services; if access is refused the utility logs the failure and does not try to bypass it. Risk Mode is **not** part of safe one-click fix.

A 1 GB/16 GB Windows 11 device is below Microsoft's official Windows 11 minimum. No optimizer can guarantee smooth performance or compatibility. On flash/eMMC, unnecessary extra writes can make things worse; the optimizer does not employ write-heavy "RAM boosters" or speculative disk cleaning.

## Publisher vs digital signature

Windows file properties carry the publisher name **Loumer Cabaluna Manreal** as version metadata. However, the executables are **not Authenticode-signed**. SmartScreen/UAC may display `Unknown publisher` until a trusted code-signing certificate is obtained and used; ordinary version metadata cannot change this.

## Build

Prerequisites: Python 3, Pillow, LLVM Clang and lld-link. No Windows SDK used.

```
python build.py          # x64
python build.py --arm64  # Windows ARM64
python verify.py         # output shape and safety checks
```

`resources.py` packages the untouched source crop `BlazeSystems-LogoV2-source-crop.png` from the owner-approved brand identity board into executable icon sizes. It aborts if the crop dimensions differ instead of guessing a new logo. The asset is a crop of the existing flat brand board, not a recovered editable vector or redesigned logo. The full brand board and its provenance are archived separately under `BlazingSystems/BlazingSystems/branding/Logo-V2`. All other content is derived from the existing v1.0 native C code in the GitHub project.

## Verification status

Built on Linux using LLVM and checked the PE architecture, imports and embedded resource section. **Windows runtime execution, admin service changes and rollback have NOT yet been verified** on a real PC. Back up important data before testing Advanced Risk Mode. This is a preview only; not a certified fault repair, malware detection, disk SMART analyzer, or full Windows compatibility solution.
