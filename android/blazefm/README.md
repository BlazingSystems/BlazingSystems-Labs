# BlazeFM

**Type:** Native Android file manager  
**Package:** `com.blazefm.blazesystems`  
**Minimum Android:** API 21  
**Stage:** Build and device validation

A lightweight Android file-manager study intended for low-memory devices.

## Implemented Source Features

- directory-first file browser;
- recursive filename search;
- rename and recursive delete;
- SHA-256 calculator;
- exact duplicate detection using staged hashing;
- visually similar-photo detection using perceptual dHash;
- storage summary;
- Android FileProvider integration;
- no analytics or advertising framework.

## Engineering Notes

The public source includes an Android 11+ storage-permission correction so legacy READ/WRITE permission requests are not used after modern all-files access has been granted.

## Validation Before Release

- complete Gradle build with a supported JDK/Android Studio;
- test storage permission flows across API 21 through current Android;
- test file-open/share URI grants;
- stress-test duplicate and image comparison on large libraries;
- run lint and low-memory regression tests;
- produce a signed release only after those checks pass.

The project is kept in Labs because a signed production APK has not been validated from this source tree.
