# BlazeFM

**Package:** `com.blazefm.blazesystems`  
**Minimum Android:** Android 5.0 / API 21  
**Version:** 1.2.0  
**Goal:** compact ES File Explorer-style file manager for older and low-memory Android devices.

BlazeFM is a native Java file manager designed to stay usable on 512 MB-class devices while covering the practical tools that made classic all-in-one Android file managers useful.

### 1.2.0 interface refresh
- modern charcoal-and-orange visual system with neutral file surfaces;
- compact top app bar with Search and overflow actions;
- rounded two-line file cards with file-type badges and metadata;
- five-destination bottom navigation for Files, Analyze, Network, Cloud and Apps;
- emphasized New action plus compact Up/Home/Paste controls;
- shared rounded buttons and surfaces across the legacy utility screens without adding Compose or a heavy UI framework.

## Implemented in 1.2.0

### Local files
- directory-first browser with file sizes and type hints;
- hidden-file toggle;
- create folder / empty text file;
- multi-select;
- batch copy, move and paste;
- rename;
- BlazeFM Trash / recycle bin with restore and permanent purge;
- favorites and recent files;
- recursive filename search;
- SHA-256 properties;
- ZIP creation and safe ZIP extraction with zip-slip protection.

### Cleanup and analysis
- exact duplicate finder: size -> sampled SHA-256 -> full SHA-256 verification;
- optional duplicate cleanup to Trash while keeping one verified copy;
- visually similar photo finder using perceptual dHash;
- storage analyzer with category totals, empty-folder count and largest-file list.

### Media, documents and APKs
- low-memory image preview;
- text preview capped at 1 MiB;
- audio and video preview using Android platform components;
- external-app fallback for unsupported formats;
- APK metadata inspection and installer hand-off;
- installed-app manager with launch, Android app-info, uninstall hand-off and APK backup.

### Network and cloud
- FTP browser with upload/download, create folder, rename and delete;
- SFTP browser with upload/download, create folder, rename and delete;
- SMB2/3-oriented browser using jcifs-ng with SMB1 disabled;
- Android Storage Access Framework browser for installed cloud/document providers such as Google Drive or Dropbox, without BlazeFM storing provider account passwords.

### Root
- optional `su` detection;
- separate root browser when root is actually available;
- secure-file preview copied into app cache;
- copy-out, rename and explicit permanent delete operations.

## Storage permission model

Android 11+ requires **All files access** for a traditional full filesystem manager. BlazeFM opens the Android permission screen instead of pretending scoped storage provides equivalent access. Cloud/document-provider browsing uses the Storage Access Framework and can work independently of all-files access.

The App Manager also requests full installed-package visibility so it can actually list and back up installed apps on Android 11+. Both all-files access and broad package visibility are policy-sensitive permissions for app stores; the GitHub sideload build keeps them because they are core file-manager functions.

## Size and performance choices

- native Java UI; no Material/Compose framework;
- no ads, analytics or telemetry SDK;
- streaming copy/ZIP/hash operations with 64 KiB buffers;
- no forced `largeHeap`;
- image decoding is sampled;
- similar-photo scans are capped at 5,000 images per folder pass to avoid pathological O(n²) memory/CPU behavior on low-RAM hardware;
- release builds use R8 and resource shrinking.

## Network security notes

- SMB1 is disabled by default in the SMB client configuration;
- SFTP excludes keyboard-interactive authentication and uses a persisted `known_hosts` file with first-use confirmation through JSch;
- FTP is supported for legacy compatibility but is plaintext by design; use SFTP when credentials or file contents need transport encryption.

## Build

The repository CI uses JDK 17, Android SDK 35 and Gradle 8.9 with Android Gradle Plugin 8.7.3.

```text
gradle :app:assembleDebug :app:assembleRelease
```

The CI publishes:

- an installable debug-signed APK for direct testing;
- an unsigned/minified release APK;
- SHA-256 checksums.

A stable production signing key is intentionally **not** committed to this public repository. Production-signed updates should use a private keystore stored in repository secrets or an offline signing workflow.
