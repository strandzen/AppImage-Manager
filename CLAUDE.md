# CLAUDE.md

This file is the authoritative reference for working on AppImage Manager.

## Project Overview

> **Disclaimer:** This project and its author are in no way affiliated with KDE or the KDE e.V. organization. While the project's goal is to provide a native-like experience on the Plasma desktop, it is an independent, community-driven project authored by the user. It should not pretend to be an official KDE product (e.g., `org.kde.*`).

**AppImage Manager** is a lightweight KDE Plasma 6 utility for installing, managing, updating, and removing AppImage files. It integrates with Dolphin via a right-click context menu plugin and provides a standalone dashboard for browsing, batch-managing, and updating all installed AppImages.

**Distribution target:** KDE Store and AUR.  
**Philosophy:** Simplicity and efficiency. No unnecessary abstractions. Every feature must justify its existence.  
**Stack:** C++20, Qt 6.9, KDE Frameworks 6.7, KirigamiAddons, Kirigami (Plasma 6 era), QML. Project version `2.0.0`.  
**License:** GPL-2.0-or-later.

### AI Assistant Guidelines

- **Plan Backups:** Always "backup" every big change or implementation plan as a `.md` file inside the `markdown/plans/` directory of the project.

#### Behavioral principles (derived from Karpathy's LLM coding observations)

**1. Think before coding.** State assumptions explicitly before implementing. If multiple interpretations exist, surface them — don't pick silently. If something is unclear, stop and ask. Push back when a simpler approach exists.

**2. Simplicity first.** Minimum code that solves the problem. No features beyond what was asked. No abstractions for single-use code. No speculative flexibility or configurability. If it could be 50 lines, don't write 200.

**3. Surgical changes.** Touch only what the request requires. Don't improve adjacent code, comments, or formatting. Match existing style even if you'd do it differently. Mention unrelated dead code — don't delete it. Remove only imports/variables/functions that *your* changes made unused.

**4. Goal-driven execution.** Transform tasks into verifiable goals before starting. For multi-step tasks, state a brief plan with per-step success criteria. Loop until verified — don't hand back unverified work.

### Roadmap (planned, not yet implemented)

- **Per-item signature badge in LibraryPage** — lazy-check signature state for each installed AppImage and display a badge in the detail pane (verification on install is done; per-library scan is the remaining piece).
- **Sandbox/bubblewrap launch option** — opt-in setting to launch AppImages under `bwrap` for filesystem isolation.

---

## Build Commands

```bash
# Configure + build (dev — Debug, installs to /usr, exports compile_commands.json)
cmake --preset dev
cmake --build --preset dev

# Install to /usr (required for Dolphin plugin discovery)
sudo cmake --install build/dev

# Reload Dolphin plugin without logout
kquitapp6 dolphin && dolphin &

# Release build
cmake --preset release
cmake --build --preset release
sudo cmake --install build/release

# Clean rebuild
rm -rf build/ && cmake --preset dev && cmake --build --preset dev

# Run unit tests
ctest --test-dir build/dev --output-on-failure
```

**When releasing:** bump `VERSION` in the top-level `CMakeLists.txt` (`project(AppImageManager VERSION X.Y.Z ...)`). This is the single source of truth — `version.h` is generated from it and `AppSettings::appVersion()` reads it at runtime. Also update the version string in this file and `GEMINI.md`, and add a `<release>` entry to `data/appimagemanager.metainfo.xml`. Forgetting any of these causes the About dialog to display the wrong version or the KDE Store validator to reject the submission.

Build output: `build/<presetName>/`. Unit tests in [tests/](tests/) cover `appimageinfo` helpers, `AppImageReader` cleanName, `AppImageCache` round-trip, and `AppImageListModel` watcher behavior. The manual checklist below still applies for UI/integration paths.

---

## Repository Layout

| Path | Contents |
|------|----------|
| [src/core/](src/core/) | Pure C++ — reader, cache, manager, settings, GitHub checker, daemon |
| [src/gui/](src/gui/) | Qt Quick backend classes (windows, models, providers, update manager, download watcher) |
| [src/dbus/](src/dbus/) | D-Bus adaptor exposing read-only model state |
| [src/plugin/](src/plugin/) | Dolphin `KAbstractFileItemActionPlugin` |
| [qml/](qml/) | All QML files (top-level, NOT under `src/`) |
| [tests/](tests/) | Unit tests (`tst_*.cpp`) |
| [data/](data/) | Desktop files, icons, autostart, .pot |

---

## Architecture — Build Targets

| Target | Type | Description |
|--------|------|-------------|
| `appimagemanager_core` | Object library | Pure C++, zero Qt Quick dependency. Reader, cache, manager |
| `appimagemanager_qml` | Shared lib + QML module `appimagemanager` | All GUI backend classes + the D-Bus adaptor + the QML files |
| `appimagemanager` (plugin `.so`) | `KAbstractFileItemActionPlugin` | Dolphin right-click plugin, auto-discovered via JSON metadata |
| `appimagemanager_bin` → `appimagemanager` | Executable | Service menu launcher + CLI. No args → dashboard. With file arg → manage window. `--daemon` → background update + download watcher |

Both the plugin `.so` and the binary link `appimagemanager_qml` — identical logic, three entry points (plugin, dashboard, daemon).

---

## Class Map

### Core (`src/core/`) — no Qt Quick

| Class | Files | Role |
|-------|-------|------|
| `AppImageInfo` | `appimageinfo.h` | Value struct: `originalName`, `cleanName`, `appId`, `appName`, `version`, `categories`, `comment`, `description`, `developerName`, `homepage`, `execArgs`, `fileSize`, `iconData`, `iconExt`, `updateInfo`, `isValid`. Also defines the `SignatureState` enum (`Unchecked=0`, `Valid=1`, `Invalid=2`, `Unsigned=3`, `GpgUnavailable=4`) used by `AppImageReader::verifySignature()`. `description` = AppStream XML content; falls back to `comment` (`.desktop Comment=`) if XML absent |
| `AppImageReader` | `appimagereader.h/.cpp` | **BLOCKING** extractor. Requires `libappimage` (in-process SquashFS, no FUSE/subprocess). Reads `.desktop` via `KDesktopFile`, then AppStream XML from `usr/share/metainfo/*.appdata.xml` or `usr/share/appdata/*.metainfo.xml` via `QXmlStreamReader`. **Always call via `QtConcurrent::run`**. Also provides `static SignatureState verifySignature(const QString &path)` — parses ELF64 section headers for `.sha256_sig`/`.sha256_hash` (falls back to `.sha1_sig`/`.sha1_hash`), writes to a `QTemporaryDir`, and invokes `gpg2`/`gpg` to verify the detached signature. Returns `Unsigned` if section absent, `GpgUnavailable` if gpg not on PATH. Safe to call from worker threads |
| `AppImageCache` | `appimagecache.h/.cpp` | Thread-safe on-disk cache (SQLite via `Qt6::Sql`, WAL mode, per-thread connection; DB path `$XDG_DATA_HOME/AppImageManager/cache.db`; one row per AppImage keyed by `MD5(path)`). **`kCacheVersion = 4`** — schema migrated from QSettings INI in v4; adds `developer_name` / `homepage`. Rows with `cache_version != 4` or stale `mtime` are returned as invalid (forces a cold re-read) |
| `AppImageManager` | `appimagemanager.h/.cpp` | File operations: `installAppImage()` (KIO::move when source is under `~/Downloads`, KIO::copy otherwise — preserves originals on USB sticks / project dirs; chmod +x on result), `createDesktopLink()`, `removeDesktopLink()`, `isDesktopLinkEnabled()`, `findCorpses()` (blocking), `removeItems()` (KIO::trash). `rebuildSycoca()` + `update-desktop-database` + `update-icon-caches` called from `createDesktopLink`/`removeDesktopLink` — covers both Plasma's service catalog and XDG-spec caches for non-Plasma launchers. `fileKey(info)` derives desktop/icon filenames: uses `appId` when it contains `.` (reverse-domain), otherwise appends a 6-char `qHash(originalName)` suffix to the cleanName slug to prevent collision between two AppImages with the same display name |
| `AppSettings` | `appsettings.h/.cpp` | QML singleton (`AppSettings`). Delegates all reads/writes to the KConfig XT-generated `AppImageManagerSettings::self()` (schema: `appimagemanagersettings.kcfg`). Properties: `applicationsPath`, `showDisclaimer`, `firstRun`, `showNotifications`, `updateFrequency`, `customUpdateDays`, `manageIconSize`, `watchDownloads`, `showInstallBox`, `accentBorders`, `verifySignatures`. Each setter calls `AppImageManagerSettings::self()->save()`. Setter validates path via `QDir::mkpath()`, emits `applicationsPathError(msg)` on failure |
| `GitHubReleaseChecker` | `githubreleasechecker.h/.cpp` | Parses `gh-releases-zsync\|owner\|repo\|...` update info, hits GitHub Releases API, emits `updateAvailable(newVersion, zsyncUrl)`, `upToDate()`, `networkFailed()`, or `failed()`. Used by both `UpdateDaemon` and `AppImageUpdateManager` — single source of truth for GitHub update logic |
| `UpdateDaemon` | `updatedaemon.h/.cpp` | Background update checker. Launched via `--daemon` CLI flag (autostart desktop file installs to `$KDE_INSTALL_AUTOSTARTDIR`). Persistent tray entry (`KStatusNotifierItem`) with "Check Now" + "Open Dashboard" actions. Scans `applicationsPath` hourly (interval via `intervalForFrequency`); timer starts only when `updateFrequency != 0` — "Never" (0) leaves the timer stopped. When frequency changes at runtime: 0 → `stop()`; non-zero → `setInterval()` + `start()`. Uses `GitHubReleaseChecker` + parallelised cold-cache `AppImageReader` reads, fires `KNotification` on update found. Owns a `DownloadWatcher` for `~/Downloads`. Registers D-Bus name `io.github.appimagemanager.Daemon` on start so the dashboard can skip duplicate download notifications. Uses `Qt6::Network` |

### GUI (`src/gui/`) — Qt Quick dependency

| Class | Files | Role |
|-------|-------|------|
| `AppImageBackend` | `appimagebackend.h/.cpp` | QML context property `backend`. Owns `AppImageInfo` + all mutable install state. Exposes metadata + ops as Q_PROPERTYs and slots. Holds `CorpseModel`. Emits `uninstallFinished`. Install flow is split: `beginInstall()` runs `AppImageReader::verifySignature()` off-thread (if `AppSettings::verifySignatures` is on) and emits `signatureCheckReady(int state)` — QML inspects the `SigState` Q_ENUM value and either calls `doInstall()` directly or shows a warning dialog first. `SigState` enum (Unchecked=0, Valid=1, Invalid=2, Unsigned=3, GpgUnavailable=4) mirrors `SignatureState` and is registered as Q_ENUM for QML access |
| `AppImageIconStore` | `appimageiconstore.h/.cpp` | Process-wide singleton holding raw icon bytes extracted from AppImages. Shared across all QML engine instances (dashboard + manage windows) so multiple windows don't re-extract the same bytes. Thread-safe write (`setIconData`) via internal lock |
| `AppImageIconProvider` | `appimageiconprovider.h/.cpp` | `QQuickImageProvider` for `image://appimage/<id>`. Reads raw bytes from `AppImageIconStore`; each provider instance owns its own per-engine `QPixmap` render cache (`m_cacheMutex` guards the `QCache` — `QCache::object` mutates LRU state even on read). SVG/PNG rendering happens between the store read and cache write. Caches rendered pixmaps keyed by `(id, size)`. Key `"icon"` = manage window single icon; key = `qHash(path)` decimal string = dashboard per-file icon |
| `CorpseModel` | `corpsemodel.h/.cpp` | `QAbstractListModel` for leftover config/cache dirs. Roles: `filePath`, `fileSize`, `isChecked`. **No `QML_ELEMENT`** — Qt 6.11 constexpr metaobject bug. Exposed only via `AppImageBackend::corpseModel` Q_PROPERTY. `checkedRole` Q_PROPERTY replaces enum access from QML |
| `AppImageWindow` | `appimagewindow.h/.cpp` | One `QQuickWindow` per AppImage path. Deduplicates via `static QHash<QString, AppImageWindow*> s_instances`. Re-opening same path raises existing window |
| `DownloadWatcher` | `downloadwatcher.h/.cpp` | Shared `~/Downloads` watcher. `KDirWatch` in `WatchFiles` mode + settle pass (poll `size`/`mtime` until stable) so we never notify mid-download. Emits `appImageFound(path, displayName)` once per file. Used by both `UpdateDaemon` and `AppImageListModel`; the dashboard suppresses its own listener while the daemon owns the well-known D-Bus name. `isSandboxed()` returns `true` when `isRunningInSandbox()` detected at construction time (env vars `FLATPAK_ID` / `SNAP`); `setEnabled(true)` is a no-op in that case — KDirWatch cannot watch paths outside the sandbox |
| `AppImageUpdateManager` | `appimageupdate.h/.cpp` | Batched update checker + downloader. `checkForUpdates(items)` — per item, fetches `.zsync` header, compares `SHA-1` (calculated off-thread via `QtConcurrent`), emits `updateFound/upToDate`. `downloadUpdate(path, zsyncUrl, ...)` — spawns `zsync2` subprocess; if `zsync2` is missing on `PATH`, falls back to a full HTTP download via the `URL:` header in the `.zsync` file. Both paths use an atomic-ish swap: `old → old.bak`, then `old.new → old`, then `QFile::remove(bak)` on success. If either rename fails, restores `.bak` to original and emits failure. A leftover `.bak` after a crash = mid-swap interruption, recoverable manually. Signals: `checkingChanged`, `checkFinished(found, failures)`, `updateFound`, `downloadProgress`, `downloadFinished` |
| `AppImageListModel` | `appimagelistmodel.h/.cpp` | Dashboard list model. Roles: `filePath`, `displayName`, `cleanName`, `appName`, `version`, `iconSource`, `hasDesktopLink`, `metadataLoaded`, `appSize`, `formattedSize`, `addedDate`, `categories`, `comment`, `description`, `developerName`, `homepage`, `updateAvailable`, `updateVersion`, `isUpdating`, `updateProgress`. Properties: `scanning`, `checkingUpdates`, `pendingLoads` (int — live count of in-flight metadata futures; QML uses this for a "Loading N…" label), `downloadWatcherSandboxed` (bool `CONSTANT` — true when `FLATPAK_ID`/`SNAP` env present). Invokables: `scan`, `refresh`, `toggleDesktopLink`, `requestRemoveAt`, `requestLaunch`, `checkForUpdates`, `downloadUpdate`, `itemData(row)` (bulk role snapshot for QML). Watches `applicationsPath` via `KDirWatch` (`WatchFiles`) and routes `created/deleted/dirty` straight to incremental inserts/removes — no debounce, no full re-scan. Reader workers dispatch through `m_readerPool` (bounded to `qBound(1, QThread::idealThreadCount() / 2, 4)`) to prevent thread storms on large libraries. Holds an `AppImageUpdateManager` and a `DownloadWatcher`. `m_generation` invalidates stale futures on `refresh()`; `m_pendingLoads` drives both the `scanning` flag and the `pendingLoads` property. `m_pathIndex` (`QHash<QString, int>`) keeps row index by path for O(1) `findRowByPath` |
| `AppImageSortFilterModel` | `appimagesortfiltermodel.h/.cpp` | Proxy over `AppImageListModel`. Sort: `SortByName=0`, `SortBySize=1`, `SortByCategory=5`, `SortByDate=9`. Filter: case-insensitive text match on `cleanName` + `appName`. `lessThan` reads typed values via `AppImageListModel::sortKey()` to skip QVariant role dispatch. Uses Qt 6.9 `beginFilterChange/endFilterChange` where applicable |
| `DashboardWindow` | `dashboardwindow.h/.cpp` | Singleton dashboard host. Creates its own `QQmlApplicationEngine`. Holds `m_listModel`, `m_proxyModel`, `m_uninstallBackend`, `m_storeModel`. `createBackend(path, withCorpses)` — uninstall backend passes `true` (triggers corpse scan after metadata load). Exposes `amStoreModel` as context property |
| `AMStoreModel` | `amstoremodel.h/.cpp` | Discover page store catalog. Two data sources: AM database (`programs/x86_64-appimages`, `◆ package : description` lines) and AppImage Hub feed (`appimage.github.io/feed.json`, 1,383 curated apps with icons, categories, authors). Both fetched in parallel; **parsing runs on worker threads** via `QtConcurrent::run` + `QFutureWatcher<QVector<StoreApp>>`; results marshalled back via `onAmParsed`/`onFeedParsed`. Disk cache in `QStandardPaths::CacheLocation` with 24h freshness check. `checkAllLoaded()` merges sources on a worker thread; `QIcon::hasThemeIcon()` is called **inside the worker** (Qt 6: `QIconLoader` is mutex-guarded, safe off main thread) to clear non-URL, non-theme iconSources before the result is moved to the main thread. `applyFilter()` dispatched to worker with `m_filterGeneration` counter to discard stale results. `storeSource` (0=All/1=Hub/2=AM) persisted in `QSettings`. SVG icons filtered at parse time — Qt's renderer crashes on CSS mesh-gradient SVGs. **Lazy-loaded: never call `load()` from constructor**; call `initialize()` once from QML when Discover page first becomes visible |

### D-Bus (`src/dbus/`)

| Class | Files | Role |
|-------|-------|------|
| `AppImageDBusAdaptor` | `appimagedbusadaptor.h/.cpp` | `QDBusAbstractAdaptor` mounted on object path `/io/github/appimagemanager/Manager`, interface `io.github.appimagemanager.Manager1`. Read-only: `InstalledAppImages()` returns `QVariantList` of `{path, name, version, hasDesktopLink}` maps; `Version()` returns project version string |

### Plugin (`src/plugin/`)

| Class | Files | Role |
|-------|-------|------|
| `AppImageActionPlugin` | `appimageactionplugin.h/.cpp` | Dolphin plugin. `actions()` returns "Manage AppImage" `QAction` that launches `appimagemanager <path>` via `QProcess::startDetached` |

---

## QML File Map

All QML lives in [qml/](qml/) (top-level, not under `src/`). Files are wired into `appimagemanager_qml` via `qt_add_qml_module` in [src/CMakeLists.txt](src/CMakeLists.txt).

| File | Type | Context / Properties |
|------|------|----------------------|
| `Theme.qml` | `pragma Singleton` QtObject | Shared `cardBorderColor` computed from `AppSettings.accentBorders` + `Kirigami.Theme`. Import as `Theme` from `appimagemanager` module. Use instead of duplicating the inline color expression |
| `ManageWindow.qml` | `ApplicationWindow` (fixed 26×22 gridUnits) | Context prop: `backend` (AppImageBackend). Drag-and-drop install — floating `dragIcon` surrogate used as drag target, original icon fades to 30% opacity during drag. On drop calls `backend.beginInstall()` (not `installAppImage`); `onSignatureCheckReady` handler routes to `doInstall()` or opens `signatureWarnDialog`. Inline `UninstallDialog` and `signatureWarnDialog` (`Kirigami.Dialog` with security icon, Ok/Cancel) |
| `DashboardWindow.qml` | `Kirigami.ApplicationWindow` | Context props: `listModel`, `proxyModel`, `dashboardController`, `amStoreModel`. Sidebar navigation (collapsible, 300ms `InOutQuad` animation) with 4 pages in a `StackLayout`: Discover (`StorePage`), Installed (`LibraryPage`), Settings (`SettingsPage`), About (`AboutPage`). Header: sidebar toggle + "AppImage Manager" heading + "Install from File" button (no app icon). Default page: Installed (index 1). Global drag-and-drop overlay for `.AppImage` files |
| `LibraryPage.qml` | `Kirigami.Page` | Context props: `listModel`, `proxyModel`. Master-detail layout. Left pane: search field, progress bar (scanning), "Loading N…" label, list with per-row chips (version/size/category/date), update indicator, busy spinner. Right pane: detail view with `displayedItem` property that lags `currentItem` — selection change triggers `SequentialAnimation` (80ms fade-out → swap content → 150ms fade-in); data updates (metadata load, update check) sync `displayedItem` immediately. `ListView` has `Controls.ScrollBar.vertical`. Action bar: Launch / Update / Remove buttons + download progress bar. Keyboard shortcuts: Return=Launch, Delete=Remove, Escape=Deselect |
| `StorePage.qml` | `Kirigami.Page` | Discover page. Context prop: `amStoreModel`. Left pane: search + sort button + source filter button (funnel → `Controls.Menu`: All/AppImage Hub/AM Database, persisted); category chips in a `Flickable` (horizontal-only, no ScrollView so no vertical clipping); `ListView` with vertical scrollbar. Right pane: detail with `displayedItem` fade-transition (same pattern as LibraryPage). Action bar: split-button `[ primary ][ ▼ ]` — primary is "Install via AM" if `hasAmScript` else "Download"; `▼` dropdown appears only when app has both sources, offering both options; "Cancel" replaces during install. Install log: colorized by keyword (error=red/warning=orange/success=green), collapsible toggle, auto-scroll. Progress bar shows install stage 0–5. Lazy init: `onVisibleChanged` calls `amStoreModel.initialize()` once |
| `SettingsPage.qml` | `Kirigami.Page` | `AppSettings` singleton. FormCard-based form. Sections: Storage & Location, Appearance, Behavior & Security, Background Updates. Custom update-interval separator + spinbox animate in/out with `Behavior on opacity { NumberAnimation { duration: 150 } }` |
| `AboutPage.qml` | `Kirigami.Page` | App info, license, links, library versions (Qt/KDE/libappimage), contributors loaded via `ContributorsModel` |
| `UninstallDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Corpse checklist + total size + confirm. `Kirigami.InlineMessage` shows KIO trash errors. Sets `backend = null` on close |
| `UpdateDialog.qml` | `Kirigami.Dialog` | Props: `appName`, `currentVersion`, `newVersion`, `proxyRow`. Signal: `downloadRequested()`. Shows version comparison + zsync2 fallback note |
| `SettingsDialog.qml` | `Kirigami.Dialog` (28×34 gridUnits, `padding: 0`) | Modal version of `SettingsPage` for ManageWindow context. Footer: "Restore Defaults" (with `Kirigami.PromptDialog` confirmation) + "Close" |
| `OnboardingDialog.qml` | `Kirigami.Dialog` | 4-step wizard shown on `AppSettings.firstRun == true`. Steps: Welcome, Applications Folder, Desktop Integration, Ready. `closePolicy: NoAutoClose`. `parentWindowWidth` property required — QML file scope does not inherit outer document IDs, so `root.width` is inaccessible; the width is passed explicitly from `DashboardWindow.qml`. `Layout.preferredWidth: 0` on all long labels prevents them from driving the dialog beyond `preferredWidth` (Qt layouts use `implicitWidth` to compute parent `implicitWidth`; wrapping text still reports its unwrapped natural width) |
| `ContributorsModel.qml` | `pragma Singleton` QtObject | Shared contributor data and fetch logic (XHR to GitHub API, bot-filtered, fallback list). Extracted from `AboutDialog.qml` and `AboutPage.qml` to eliminate duplication. Import as `ContributorsModel` from `appimagemanager` module |

---

## Data Flows

### Manage window: open → install

```
AppImageWindow::open(path)
  └─ AppImageBackend(path)            starts QtConcurrent::run
       └─ AppImageReader::read()      BLOCKING, worker thread
            ├─ libappimage path: in-process SquashFS (preferred)
            └─ squashfuse path: subprocess mount → parse → fusermount3 unmount
       └─ onMetadataReady(info)       main thread; emits metadataLoadedChanged
  └─ ManageWindow.qml                 BusyIndicator until metadataLoaded; then icon + info shown
  └─ user drags icon to folder
       └─ backend.beginInstall()
            ├─ if verifySignatures OFF → doInstall() immediately
            └─ if verifySignatures ON  → QtConcurrent::run(AppImageReader::verifySignature)
                 └─ QFutureWatcher::finished → signatureCheckReady(state)
                      ├─ state Valid/Unchecked/GpgUnavailable → doInstall()
                      └─ state Unsigned/Invalid → signatureWarnDialog.open()
                           └─ user clicks OK → doInstall()
       └─ doInstall() → AppImageManager::installAppImage()
                 ├─ KIO::move  if source under ~/Downloads
                 └─ KIO::copy  otherwise (preserves USB / project sources) + chmod +x on result
            └─ onInstallJobFinished → isInstalled = true → folder column fades → icon centers
```

### Dashboard: initial scan + live watching

```
DashboardWindow::setupAndShow()
  └─ AppImageListModel::scan()
       └─ QDir(applicationsPath).entryInfoList(*.AppImage / *.appimage)
       └─ main-thread cache pre-check; skip worker spawn on warm cache hit
       └─ otherwise QtConcurrent::run(AppImageReader::read())
            └─ QFutureWatcher::finished
                 → if (gen != m_generation) drop the result
                 → applyMetadata(row, info); dataChanged(row, row)
                 → if (--m_pendingLoads == 0) scanning = false
  └─ KDirWatch (WatchFiles) on applicationsPath
       ├─ created(path)  → onFileCreated  (begin/endInsertRows + loadMetadataForRow)
       ├─ deleted(path)  → onFileDeleted  (begin/endRemoveRows)
       └─ dirty(path)    → onFileDirty    (re-read metadata for one row)
  └─ DownloadWatcher (when watchDownloads is on; suppressed if daemon D-Bus name is registered)
       └─ appImageFound(path, displayName) → KNotification "Manage" → ManageWindow
```

### Update check + download

```
listModel.checkForUpdates()
  └─ AppImageUpdateManager::checkForUpdates({CheckItem ...})
       └─ per item:
            ├─ gh-releases-zsync → GitHubReleaseChecker (Releases API)
            └─ plain zsync| URL  → fetch .zsync header, parse SHA-1, hash local file off-thread
       └─ updateFound(path, version, zsyncUrl) → role updates → UpdateDialog
  └─ user confirms → listModel.downloadUpdate(row)
       └─ AppImageUpdateManager::downloadUpdate(...)
            ├─ zsync2 on PATH         → QProcess zsync2 -i old -o old.new URL
            └─ zsync2 missing         → fetch .zsync header for URL:, full HTTP download to old.new
            └─ on success: rename old→old.bak, rename old.new→old, remove bak, chmod +x, downloadFinished(path, true)
                           (on rename failure: restore bak, remove new, emit failure)
```

### Discover page: store catalog load

```
StorePage.qml onVisibleChanged (first open)
  └─ amStoreModel.initialize()
       └─ AMStoreModel::load(forceNetwork=false)
            ├─ ~/.cache/AppImageManager/am-database.txt  fresh? → read file
            │   OR GET https://raw.githubusercontent.com/ivan-hc/AM/main/programs/x86_64-appimages
            │        → save cache
            └─ ~/.cache/AppImageManager/am-feed.json     fresh? → read file
                OR GET https://appimage.github.io/feed.json
                     → save cache
       (two QtConcurrent::run workers in parallel)
       ├─ parseAmDatabaseSync(rawText)  → QVector<StoreApp>  [worker thread]
       └─ parseFeedJsonSync(data)       → QVector<StoreApp>  [worker thread]
            └─ PNG/JPG/WEBP icons only — SVG paths skipped (crash guard)
  └─ onAmParsed / onFeedParsed  [main thread via QFutureWatcher::finished]
       └─ checkBothLoaded()
            └─ mergeDatabases()   [worker thread — QIcon::hasThemeIcon() called here; Qt 6 QIconLoader is mutex-guarded]
                 └─ cross-ref by normalizedName; Hub metadata wins on description/categories
            └─ applyFilter()  → QtConcurrent::run worker (filter + localeAwareCompare sort)
                 └─ m_filterGeneration discards stale results if filter changed mid-flight
                 └─ beginResetModel / endResetModel on main thread when done
  └─ amStoreModel.loading = false → BusyIndicator hides → list appears
```

### Uninstall flow

```
user clicks Remove
  └─ backend.findCorpses()           async QtConcurrent::run
       └─ AppImageManager::findCorpses(info)
            scans ~/.config, ~/.local/share, ~/.cache for dirs matching appName/appId
            filters via ~60-entry blacklist (plasma, kwin, python, git, etc.)
       └─ CorpseModel::setCorpses()  populates model; UninstallDialog shows checkboxes
  └─ user selects items; confirms
  └─ backend.removeAppImageAndCorpses(paths)
       └─ AppImageManager::removeItems()  → KIO::trash() (never permanent delete)
       └─ AppImageManager::removeDesktopLink() if exists
       └─ emits uninstallFinished → ManageWindow closes / KDirWatch fires removal into dashboard
```

---

## Key Conventions

### Threading
- `AppImageReader::read()` and `AppImageManager::findCorpses()` are **always blocking** — call only via `QtConcurrent::run`.
- `AppImageListModel` dispatches reader workers through `m_readerPool` (private `QThreadPool`, cap `qBound(1, idealThreadCount / 2, 4)`) — not the global pool — to prevent thread storms on large libraries.
- `AppImageUpdateManager` hashes local AppImages off-thread via `QtConcurrent::run`.
- All async results marshal back to main thread via `QFutureWatcher::finished`.
- Never touch `m_items`, `m_scanning`, or `m_generation` from worker threads.

### Deletion
- User files go to `KIO::trash()` — **never** `QFile::remove()` or `QDir::removeRecursively()`.
- Connect `KJob::result()` before calling `job->start()`.

### Backend lifetimes
- `AppImageWindow` owns one `AppImageBackend` for its entire lifetime.
- `DashboardWindow::m_uninstallBackend`: use `deleteLater()` when replacing, never raw `delete` — in-flight `QFutureWatcher` lambdas capture `this`.
- `AppImageIconProvider` is parented to the backend (`iconProvider->setParent(backend)`) so it outlives engine teardown. Pass pointer to engine via `addImageProvider()`.

### Desktop file paths
- `AppImageManager::desktopFilePath(info)` → `~/.local/share/applications/<appId>.desktop`
- `AppImageManager::iconFilePath(info)` → `~/.local/share/icons/<appId>.<ext>`
- Both derived from `AppImageInfo::appId`.

### QML patterns
- All windows: `Kirigami.Theme.colorSet: Kirigami.Theme.Window` + `Kirigami.Theme.inherit: false`.
- Animated show/hide: `visible: opacity > 0` + `Behavior on opacity { NumberAnimation }`.
- Dialogs: take a `backend` (or domain-specific) property, reset it to `null`/defaults in `onClosed`.
- `UninstallDialog` / `UpdateDialog` are dialogs, not windows — instantiated inline in their parent.
- Use `listModel.itemData(row)` for bulk role reads; per-role `.data()` round-trips dominate the detail pane otherwise.
- Shared theme values (e.g. `cardBorderColor`) go in `Theme.qml` singleton — do not duplicate the inline expression in each QML file.

### Shared helpers (`appimageinfo.h`)

Four `inline` utilities defined at file scope in `appimageinfo.h` (included by everything):

- `normalizeVersion(v)` — strips leading `v`/`V` from version strings.
- `isNewerVersion(remote, local)` — compares dot-split version tuples; returns true if remote > local.
- `kAppImageFilters()` — returns `{ "*.AppImage", "*.appimage" }` as a static `QStringList`. Use everywhere instead of inline literals.
- `isRunningInSandbox()` — returns `true` when `FLATPAK_ID` or `SNAP` env var is set. Result is cached in a `static const bool` evaluated once at program start. Used by `DownloadWatcher` to skip KDirWatch setup and by QML via `listModel.downloadWatcherSandboxed`. No extra include needed — `qgetenv` is covered by `<QByteArray>`.

### D-Bus
- **Daemon name** `io.github.appimagemanager.Daemon` — registered by `UpdateDaemon` on start. `AppImageListModel`'s download watcher path checks this via `QDBusConnection::sessionBus().interface()->isServiceRegistered(...)` and silently returns if the daemon is running — prevents duplicate download notifications.
- **Read-only adaptor** `io.github.appimagemanager.Manager1` on `/io/github/appimagemanager/Manager` (mounted by the dashboard process) — exposes `InstalledAppImages()` and `Version()` for external integrations.

### AppImage type support
- **Type 2** (SquashFS, ELF+magic 0x414902): primary target, full support.
- **Type 1** (ISO9660): detected, best-effort only — not a priority, may have extraction gaps.

### i18n
- QML: `i18n("string")` / `i18n("string %1", value)`. Enabled by `KLocalization::setupLocalizedContext(engine)`.
- C++: `i18n()` from `<KLocalizedString>`.
- `.pot` regenerated by CMake (`ki18n_install(po)`). Add language: copy `po/appimagemanager.pot` → `po/<lang>/appimagemanager.po`.

---

## Known Pitfalls

| Issue | Detail |
|-------|--------|
| **Clangd false positives** | `appimagemanager_qml_export.h` is generated into `build/dev/src/`. Clangd reports cascade errors on all GUI files unless `compile_commands.json` is indexed. Dev preset has `CMAKE_EXPORT_COMPILE_COMMANDS=ON` — run `cmake --preset dev` once to generate it. All `unknown_typename` errors on Qt/KDE macros are false positives if the build succeeds |
| **`Kirigami.Units.fonts` missing** | Does not exist in Kirigami 6. Use `Kirigami.Theme.smallFont` (a `font` object) |
| **`anchors.fill` + `drag.target`** | Anchors override x/y set by drag system. Items that are drag targets must use explicit `width`/`height`, not `anchors.fill` |
| **`AnchorChanges` in States** | Does not support `anchors.fill` shorthand. Only individual sides: `anchors.left`, `anchors.top`, etc. |
| **`clip: true` on rounded Rectangle** | Clips to bounding rect, ignores `radius`. Cannot achieve rounded clip this way |
| **`beginFilterChange`/`endFilterChange`** | Available since Qt 6.9 (now the project minimum). Prefer the new API for filter mutations that need begin/end semantics; `invalidateFilter()` still works for the common case |
| **`CorpseModel` QML_ELEMENT** | Do not add `QML_ELEMENT` — triggers Qt 6.11 constexpr metaobject `static_assert` |
| **KDBusService mode** | Bifurcated by invocation: file/daemon modes use `Multiple` (Dolphin can open several manage windows in parallel; the daemon registers its own well-known D-Bus name). Dashboard mode (`appimagemanager` / `--dashboard`) uses `Unique`; a second invocation pings `activateRequested` and raises the existing window via `DashboardWindow::open()` |
| **`AppImageCache` version** | Cache is versioned (`kCacheVersion = 4`, SQLite-backed). Adding new fields to `AppImageInfo` that must be persisted requires bumping `kCacheVersion`, adding a column to the `CREATE TABLE metadata` schema, and adding the field to both `load()` and `store()`. Forgetting any of the three silently returns empty strings for the new field |
| **`KDirWatch` over `QFileSystemWatcher`** | Both `AppImageListModel` and `DownloadWatcher` use `KDirWatch` in `WatchFiles` mode. It delivers per-file `created(path)` / `deleted(path)` / `dirty(path)` (deduplicated internally), so the model does incremental inserts/removes instead of debouncing + full re-scans. Pre-existing files do NOT replay on `addDir`, so seeding by hand is unnecessary. `dirty(path)` fires on the watched dir itself when a child is added or removed — handlers must `if (path == m_watchedDir) return;` to avoid double-handling |
| **`m_generation` invalidation** | `AppImageListModel::refresh()` increments `m_generation`; in-flight metadata futures capture the generation they started in and drop their result if the model has moved on. Always check `gen != m_generation` before mutating `m_items` from a future continuation |
| **`zsync2` optional** | `AppImageUpdateManager::downloadUpdate()` probes `PATH` for `zsync2`; if missing, it falls back to a plain HTTP download of the binary URL parsed from the `.zsync` control file. Do not assume zsync2 is installed |
| **`QLatin1String` and multi-byte UTF-8** | `QLatin1String("• ")` misinterprets the bullet (U+2022, 3 UTF-8 bytes) as 3 Latin-1 chars. Always use `QStringLiteral("• ")` for non-ASCII string literals |
| **Kirigami Dialog `preferredHeight` binding loop** | Binding `preferredHeight` to `dialog.window.height` (e.g. `preferredHeight: dialog.window.height - someOffset`) creates a circular dependency: Kirigami computes the dialog's `y` from `window.height` + its own `height`, which depends on `preferredHeight`. Result: `Binding loop detected for property "y"` at runtime, followed by SIGABRT via `free(): invalid size`. Use a fixed `Kirigami.Units.gridUnit * N` value instead, or a property that does not involve the dialog's own position or size |
| **`pendingLoads` dispatch ordering** | In `AppImageListModel::refresh()`, `m_pendingLoads` must be assigned **before** any `loadMetadataForRow()` calls. The cache fast-path decrements `m_pendingLoads` synchronously on the main thread; if the assignment comes after, those decrements race against the final value and leave a stuck non-zero count ("Loading N…" never clears). Always: compute the expected count → assign → emit → dispatch |
| **Plasma `ItemDelegate` position-aware insets** | The Plasma/Kirigami QQC2 style applies a larger `bottomInset` to the last item in a `ListView`, making its hover/selection highlight appear vertically squashed. Fix: set `topInset`, `bottomInset`, `leftInset`, `rightInset`, and `verticalPadding` explicitly on the delegate so all rows use identical geometry |
| **`AMStoreModel` lazy init** | Never call `load()` from the constructor — the AM database + feed.json are ~500 KB of network + JSON parsing that blocks the main thread. Call `amStoreModel.initialize()` from `StorePage.qml`'s `onVisibleChanged` (guarded by `!storeInitialized`). `initialize()` is idempotent; `sync()` forces a re-fetch regardless |
| **`AMStoreModel` SVG crash** | `parseFeedJsonSync()` skips any icon whose path does not end in `.png`/`.jpg`/`.jpeg`/`.webp`. Feed.json contains SVGs that use CSS mesh gradients — Qt's SVG renderer `free(): invalid size` → SIGABRT when it encounters them. Never remove this extension filter |
| **`AMStoreModel` icon base URL** | Icon paths in `feed.json` are relative paths like `86Box/icons/512x512/86box.png`. The correct base is `https://appimage.github.io/database/` — producing `https://appimage.github.io/database/86Box/icons/512x512/86box.png`. Using `https://appimage.github.io/` (without `database/`) returns 404 for all icons |
| **`OnboardingDialog` sizing** | `preferredHeight` must be a fixed `gridUnit * N` — binding it to `root.height` causes the Kirigami Dialog `y` binding loop → SIGABRT (see **Kirigami Dialog `preferredHeight` binding loop** pitfall). For `preferredWidth`, `root` inside `OnboardingDialog.qml` does NOT resolve to the outer `DashboardWindow.qml`'s `id: root` — QML file scope does not inherit outer document IDs. Pass window width via an explicit property: `parentWindowWidth: root.width` from the instantiating file. Additionally, `Controls.Label` with `wrapMode: Text.WordWrap` reports its natural (unwrapped) line width as `implicitWidth`; Qt Layouts use `implicitWidth` to compute parent `implicitWidth`, which makes `Kirigami.Dialog` expand beyond `preferredWidth`. Fix: set `Layout.preferredWidth: 0` on all long labels |
| **`AMStoreModel` worker threading rules** | `parseAmDatabaseSync()` and `parseFeedJsonSync()` are `static` pure functions. `QIcon::hasThemeIcon()` is called inside the `checkAllLoaded()` worker (not in parse functions — they may run before Qt's event loop is fully up). Qt 6's `QIconLoader` uses an internal mutex so this is safe off the main thread. **Do not move it back to the main thread** — iterating ~3000 entries with theme lookups blocks the UI for hundreds of milliseconds. `applyFilter()` dispatches to a worker via `QtConcurrent::run`; uses `m_filterGeneration` (incremented on each call) so the `QFutureWatcher::finished` lambda silently discards stale results |
| **`Controls.ScrollView` chips clipping** | `Controls.ScrollView` reserves vertical space for its scrollbar even with `AlwaysOff` policy, clipping chip text from below. For horizontal-only scrolling with auto-height, use a `Flickable` with `flickableDirection: Flickable.HorizontalFlick` and `Layout.preferredHeight: contentRow.implicitHeight` instead |
| **Detail pane `displayedItem` pattern** | `LibraryPage` and `StorePage` both use a `displayedItem` property that lags behind `currentItem`. On selection change: `currentItem` updates immediately (so action-bar logic stays correct), then `SequentialAnimation` fades `detailInner` to 0 → ScriptAction updates `displayedItem` → fades back to 1. Data-only updates (metadata loaded, update check result) bypass the animation and sync `displayedItem` directly to avoid flickering on in-place updates |
| **`installAppImage()` removed from `AppImageBackend`** | The old single-step `installAppImage()` slot is gone. Use `beginInstall()` → wait for `signatureCheckReady(state)` → call `doInstall()`. QML in ManageWindow already does this via the `Connections` handler. Any test or external code that called `backend.installAppImage()` directly must be updated |
| **`fileKey()` hash suffix is a 2.0 breaking change** | Apps without a reverse-domain `appId` now get a `_<hash>` suffix in their `.desktop` and icon filenames. Pre-2.0 desktop links (without the suffix) will no longer be detected by `isDesktopLinkEnabled()`. Users must re-toggle those links once after upgrading. Do not add a migration fallback that checks the legacy path — it would mask the slug collision bug the change was made to fix |
| **`verifySignature()` requires `gpg` or `gpg2` on PATH** | If neither is found, `verifySignature()` returns `GpgUnavailable`. The QML treats this like `Valid` (proceeds without dialog) so users without gpg installed are not blocked. Don't treat `GpgUnavailable` as a failure — log a debug message at most |

---

## Testing Checklist (Manual)

After any change, build and verify:

1. `cmake --build --preset dev` → zero errors. `ctest --test-dir build/dev --output-on-failure` → all green.
2. `sudo cmake --install build/dev` + `kquitapp6 dolphin && dolphin &`.
3. Right-click an uninstalled `.AppImage` → "Manage AppImage" → window opens at correct fixed size.
4. Drag icon to folder → install animates → folder column fades → icon centers.
5. Click installed icon → app launches.
6. Click Remove → `UninstallDialog` opens inline → corpse list populates → trash works → window closes.
7. `appimagemanager` (no args) → DashboardWindow opens, scans, shows list with icons. Drop a new `.AppImage` into the apps dir → row inserts live (no full reload).
8. Dashboard search → filters live. Sort by name/size/category/date → list re-orders with animation.
9. Dashboard delete button → `UninstallDialog` → list refreshes after trash.
10. Dashboard "Check for updates" → progress bar; if any update found, `UpdateDialog` opens → confirm → download progresses; both `zsync2` present and absent (rename it temporarily) should succeed.
11. Settings → browse button opens folder picker → valid path saved → invalid path shows error.
12. Settings → disclaimer toggle persists across restart.
13. Select an AppImage with AppStream metadata (e.g. ProtonUp-Qt) → description text visible below chips in details pane; only inner description scrollbar visible, no outer scrollbar; no horizontal scrollbar.
14. Settings → "Notify when an AppImage is downloaded" on → copy a `.AppImage` into `~/Downloads` → KDE notification fires after settle pass → click "Manage" → ManageWindow opens for that file.
15. Toggle download notification off → copy another AppImage to `~/Downloads` → no notification fires.
16. Start daemon (`appimagemanager --daemon`) → tray entry appears with "Check Now" + "Open Dashboard". Open dashboard while daemon runs → drop a file in `~/Downloads` → only one notification fires (daemon wins).
17. AboutDialog: open dialog → disable network (`nmcli networking off`) → reopen About → wait 8 s → spinner hides, fallback contributors shown, no SIGABRT. Re-enable network, reopen → real contributors load.
18. Sandbox: run `FLATPAK_ID=test appimagemanager`, open Settings → "Download watching is unavailable in sandboxed environments." InlineMessage visible below toggle. Copy a `.AppImage` to `~/Downloads` → no notification fires.
19. Update: trigger update on a test AppImage → verify `.bak` file created mid-swap; on success `.bak` removed. Kill process during swap → `.bak` present afterward for manual recovery.
20. Launch app → navigate to Library immediately → no loading delay; Discover tab NOT yet loaded (no network traffic at startup).
21. Click Discover → spinner visible immediately → list populates after a few seconds with no UI freeze. Close and reopen Discover → loads instantly from cache.
22. Discover search → type "firefox" → results filter live. Sort A→Z / Z→A. Source filter button (funnel) → select "AppImage Hub" → only Hub apps shown; "AM Database" → only AM apps; "All" restores full list. Filter button highlighted when non-default source active.
23. Discover: select app with both AM + Hub → `[ Install via AM ][ ▼ ]` split button; `▼` shows "Install via AM" and "Download from Hub". App with only AM → plain "Install via AM" button. App with only Hub download → plain "Download" button.
24. Discover: category chips row scrolls horizontally by dragging; no vertical text clipping; chips respond with scale pulse on click.
25. Discover: click Install → log shows, progress bar advances (Fetching→Downloading→Installing→Done); error lines red, success lines green. Cancel mid-install → "Installation cancelled." in log, button resets.
26. Settings → enable "Verify GPG signatures". Drag an unsigned AppImage to install → `signatureWarnDialog` appears with "No Signature Found" title and security-medium icon. Click Cancel → no install. Click OK → install proceeds normally.
27. With signature verification on, drag an AppImage that has a valid GPG signature → no dialog, installs directly.
28. With signature verification OFF → drag any AppImage → installs immediately, no dialog regardless of signature state.
29. Install two AppImages with the same display name but different filenames, neither having a reverse-domain `appId` → both get unique `.desktop` files in `~/.local/share/applications/` (slugs differ by hash suffix); `isDesktopLinkEnabled` correctly tracks each independently.
