# AppImage Manager: Functional Rundown

This document provides a comprehensive technical rundown of all core functionalities of the **AppImage Manager** project. Each function is analyzed and classified as **Working**, **Working with errors / Best-effort**, or **Not working (Earmarked)**.

---

## 🟢 1. Working (Fully Functional & Validated)

The following functions are fully implemented, verified via unit tests/compilation, and operate as advertised:

### A. Core File Operations & Installation (`src/core/appimagemanager.cpp`)
* **Copy/Move Installs**: Copies AppImage binaries to `~/Applications` (or custom paths), utilizing native `KIO::move` if sourced from `~/Downloads` and `KIO::copy` otherwise.
* **Make Executable**: Automatically modifies permissions to `chmod +x` after copy/move jobs finish successfully.
* **Desktop Menu Integration (`createDesktopLink`)**: Writes a `.desktop` integration launcher file to `~/.local/share/applications/` and saves the extracted icon to `~/.local/share/icons/AppImages/`.
* **KDE Cache Sync**: Invokes `kbuildsycoca6` immediately after link mutations so the application launcher updates without desktop logouts.

### B. Metadata Extraction & Parsing (`src/core/appimagereader.cpp`)
* **In-Process Type 2 SquashFS Extraction**: Parsed in-memory using `libappimage` when built with `HAVE_LIBAPPIMAGE` (sub-100ms scans, zero mount/FUSE dependencies).
* **Out-of-Process Fallback Extraction**: Runs `--appimage-extract` as a subprocess to a temporary directory if `libappimage` is missing.
* **KDesktopFile Integration**: Reads embedded `.desktop` metadata including translations, comment fields, versions, and categories.
* **AppStream XML Parsing**: Extracts descriptions, developer names, and homepages via standard `QXmlStreamReader`.
* **⚡ Localized Descriptions**: Parses `xml:lang` tags and matches them against `QLocale::system().uiLanguages()`, falling back gracefully to English or any available translation.
* **Icon Extraction**: Parses PNG, SVG, and `.DirIcon` assets, passing them safely to the thread-safe `AppImageIconProvider`.

### C. SQLite Cache System (`src/core/appimagecache.cpp`)
* **Fast Caching**: SQLite-backed database (`cache.db`) in WAL mode utilizing point-lookups (PRIMARY KEY by MD5 of file path). Automatically invalidates entries if the file's `mtime` changes.
* **Thread Safety**: Mapped using thread-specific SQLite connection pools, preventing collisions between worker threads scanning in parallel.

### D. Active Update Checking (`src/core/githubreleasechecker.cpp` & `src/gui/appimageupdate.cpp`)
* **zsync Header Parsing**: Downloads the first 2KB of `.zsync` files to check hashes.
* **⚡ Async Hashing**: Computes local SHA-1 hashes in parallel worker threads, preventing GUI freezes.
* **⚡ GitHub Releases RSS Fallback**: Automatically bypasses GitHub API rate limits (HTTP 403) by falling back to the public XML Atom feed (`releases.atom`) and parsing the latest tag/zsync links.

### E. In-App Updating (`src/gui/appimageupdate.cpp`)
* **Delta-Updates**: Spawns `zsync2` as a subprocess to update binary blocks.
* **⚡ Standard HTTP Streaming Fallback**: Standard HTTP streaming is triggered dynamically if `zsync2` is missing. It parses the target URL, downloads the file in chunks to avoid high RAM usage, swaps old files, and sets permissions.

### F. Passive Background Daemon (`src/core/updatedaemon.cpp`)
* **Downloads Folder Watcher**: Watches `~/Downloads` for AppImages using passive `KDirWatch` (inotify-based) and triggers KDE notifications with a "Manage" link.
* **D-Bus Prevention**: Registers D-Bus service `io.github.appimagemanager.Daemon` to avoid duplicate download notifications when the dashboard is running.

### G. GUI Applications & Dialouges (`src/gui/` & `qml/`)
* **Manage Window (`ManageWindow.qml`)**: Clean fixed-grid overlay allowing simple drag-and-drop installations.
* **Dashboard Window (`DashboardWindow.qml`)**: Master-detail view supporting live search filters and animated sorting (name, size, categories, and dates).
* **Uninstall Dialogue (`UninstallDialog.qml`)**: Fuzzy corpse scanner checking custom cache/configs directories matching the app ID/name, trashing files safely via `KIO::trash()`.
* **Storage Dialogue (`StorageDialog.qml`)**: Lists binary locations and config folders, linking directly to the native Dolphin file manager.

---

## 🟡 2. Working with Errors / Best-Effort

The following features operate but are subject to limitations or external failures:

### A. Type 1 AppImage Support (`src/core/appimagereader.cpp`)
* **Status**: **Working with errors / Best-effort**
* **Detail**: Type 1 ISO9660-based AppImages are detected and parsed using fallback extraction. However, due to severe ISO format variations and parsing constraints under `libappimage`, Type 1 files suffer from frequent extraction failures and are not guaranteed to display descriptions or icons.

---

## 🔴 3. Not Working (Earmarked for Future Development)

The following roadmap items are **not implemented** in the current code, but are documented and earmarked for future development cycles:

### A. Signature Verification (Roadmap)
* **Status**: **Not working (Earmarked)**
* **Detail**: There is currently no implementation verifying embedded GPG cryptographic signatures of downloaded AppImages before installation.

### B. Integrated Sandboxing (Roadmap)
* **Status**: **Not working (Earmarked)**
* **Detail**: Sandboxing execution layers (such as prepending commands with `firejail` or `bubblewrap` inside generated desktop files) are not yet implemented.
