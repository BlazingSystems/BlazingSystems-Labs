# BlazeFM

**Package:** `com.blazefm.blazesystems`  
**Minimum Android:** Android 5.0 / API 21  
**Version:** 1.3.3  
**Goal:** compact ES File Explorer-style file manager for older and low-memory Android devices.

BlazeFM is a native Java file manager designed to stay usable on 512 MB-class devices while covering the practical tools that made classic all-in-one Android file managers useful.

### 1.3.3 cleanup-results UX
- exact-duplicate groups open in a dedicated native Material results browser instead of a long report dialog;
- similar-photo pairs open in the same native results browser with direct access to either image;
- duplicate cleanup remains SHA-256 verified and keeps the first verified copy in each group;
- cleanup reports are written to app cache rather than pushed through large Intent extras.

### 1.3.2 search and navigation polish
- dedicated native Material search screen instead of a modal text prompt;
- RecyclerView search results with scoped search context and progress feedback;
- direct return-to-folder navigation from search results;
- quick-access Favorites and Recent entry points on the Files home surface;
- dedicated Material Trash screen with restore, permanent delete and empty-state handling;
- Analyzer redesigned from a text report into metrics, category usage bars and largest-file rows;
- preserved the v1.3.1 flattened, borderless Material file-row and secondary-screen visual system.

### 1.3.1 visual polish
- flatter Material file rows and secondary browser rows with reduced card chrome;
- dedicated search surface in the Files header;
- softer storage/category surfaces and thumbnail-first grid tiles;
- filled Material selection state instead of outlined web-style cards;
- friendly `Internal storage › …` breadcrumbs instead of always showing raw filesystem paths;
- consistent borderless surfaces across Analyzer, Cloud and media preview.

### 1.3.0 native UI architecture
- replaced the programmatically assembled main browser with native Android XML layouts;
- Material 3 day/night theme with real light and dark palettes;
- RecyclerView-based list/grid browser instead of ListView/GridView swapping;
- MaterialToolbar plus contextual selection toolbar, BottomNavigationView, assist Chips and Extended FAB;
- real Android vector drawables for file types and navigation/actions instead of emoji or Unicode glyphs;
- native Material dialogs for permissions, destructive confirmations, results, APK information and errors;
- Analyzer, Preview, App Manager, Cloud provider/browser, Root Browser and SMB/FTP/SFTP Network screens migrated to XML/Material surfaces;
- shared RecyclerView row architecture across local, cloud, remote, root and app-management browsers;
- shared Material BottomSheet action menus and Material text prompts replace the former hand-built full-screen sheets and Unicode pseudo-icons;
- removed the superseded ListView adapters and collapsed the old programmatic Ui helper layer to a density conversion utility only;
- storage dashboard and category shortcuts for Images, Video, Audio, Documents and APKs;
- retained bounded asynchronous thumbnail decoding and the existing low-memory file-operation engine;
- Android 5.0 / API 21 remains the minimum supported platform.

### 1.2.0 interface refresh
- modern charcoal-and-orange visual system with neutral file surfaces;
- compact top app bar with Search and overflow actions;
- rounded two-line file cards with file-type badges and metadata;
- five-destination bottom navigation for Files, Analyze, Network, Cloud and Apps;
- emphasized New action plus compact Up/Home/Paste controls;
- shared rounded buttons and surfaces across the legacy utility screens without adding Compose or a heavy UI framework;
- full-screen card-based Tools & Utilities panel instead of the legacy stock alert list;
- redesigned image/text/video/audio preview shell with share, external-open and file-detail actions;
- low-memory asynchronous image thumbnails in file rows with a bounded 4 MiB cache;
- dedicated empty-folder state with a Create action;
- custom bottom file-action sheet replacing the old long-press stock menu;
- persistent List/Grid browser modes with thumbnail-aware grid cards;
- persistent Sort controls for name, date, size and file type;
- BlazeFM-styled bottom sheets for Create, Rename and Search instead of stock Android prompts.

### 1.2.0 reliability hardening
- Cloud, Remote, Root exports and APK backups use Android's Downloads collection on Android 10+ instead of assuming unrestricted filesystem writes;
- recursive copy, move, search, analyzer, delete and ZIP operations do not follow local symbolic links;
- copy/move rejects putting a directory inside itself;
- failed copy, ZIP and extract operations clean up incomplete output where practical;
- ZIP extraction keeps zip-slip protection and stops before exhausting available device storage;
- similar-photo results are memory-bounded while still reporting the total matches found;
- SFTP password text is cleared after a successful connection;
- removing a saved SAF provider also releases its persisted URI permission;
- APK install on Android 8+ routes to the per-app unknown-sources permission when required.

## Implemented in 1.3.2

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

- native Java with XML/RecyclerView and Material Components; no Compose runtime;
- no ads, analytics or telemetry SDK;
- streaming copy/ZIP/hash operations with 64 KiB buffers;
- no forced `largeHeap`;
- image decoding is sampled;
- similar-photo scans are capped at 5,000 images per folder pass and retain only the best 2,000 matching pairs in memory;
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
