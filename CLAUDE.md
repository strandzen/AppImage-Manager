# CLAUDE.md

This file is the authoritative reference for working on AppImage Manager.

## Project Overview

> **Disclaimer:** This project and its author are in no way affiliated with KDE or the KDE e.V. organization. While the project's goal is to provide a native-like experience on the Plasma desktop, it is an independent, community-driven project authored by the user. It should not pretend to be an official KDE product (e.g., `org.kde.*`).

**AppImage Manager** is a lightweight KDE Plasma 6 utility for installing, managing, updating, and removing AppImage files. It integrates with Dolphin via a right-click context menu plugin and provides a standalone dashboard for browsing, batch-managing, and updating all installed AppImages.

**Distribution target:** KDE Store and AUR.  
**Philosophy:** Simplicity and efficiency. No unnecessary abstractions. Every feature must justify its existence.  
**Stack:** C++20, Qt 6.9, KDE Frameworks 6.7, Kirigami (Plasma 6 era), QML. Project version `1.3.0`.  
**License:** GPL-2.0-or-later.

### Roadmap (planned, not yet implemented)

- **Signature verification** — verify AppImage signatures before install.

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
| `AppImageInfo` | `appimageinfo.h` | Value struct: `originalName`, `cleanName`, `appId`, `appName`, `version`, `categories`, `comment`, `description`, `developerName`, `homepage`, `execArgs`, `fileSize`, `iconData`, `iconExt`, `updateInfo`, `isValid`. `description` = AppStream XML content; falls back to `comment` (`.desktop Comment=`) if XML absent |
| `AppImageReader` | `appimagereader.h/.cpp` | **BLOCKING** extractor. Requires `libappimage` (in-process SquashFS, no FUSE/subprocess). Reads `.desktop` via `KDesktopFile`, then AppStream XML from `usr/share/metainfo/*.appdata.xml` or `usr/share/appdata/*.metainfo.xml` via `QXmlStreamReader`. **Always call via `QtConcurrent::run`** |
| `AppImageCache` | `appimagecache.h/.cpp` | Thread-safe on-disk cache (SQLite via `Qt6::Sql`, WAL mode, per-thread connection; DB path `$XDG_DATA_HOME/AppImageManager/cache.db`; one row per AppImage keyed by `MD5(path)`). **`kCacheVersion = 4`** — schema migrated from QSettings INI in v4; adds `developer_name` / `homepage`. Rows with `cache_version != 4` or stale `mtime` are returned as invalid (forces a cold re-read) |
| `AppImageManager` | `appimagemanager.h/.cpp` | File operations: `installAppImage()` (KIO::move when source is under `~/Downloads`, KIO::copy otherwise — preserves originals on USB sticks / project dirs; chmod +x on result), `createDesktopLink()`, `removeDesktopLink()`, `isDesktopLinkEnabled()`, `findCorpses()` (blocking), `removeItems()` (KIO::trash). `rebuildSycoca()` is called automatically from createDesktopLink / removeDesktopLink |
| `AppSettings` | `appsettings.h/.cpp` | QML singleton (`AppSettings`). KSharedConfig → `appimagemanagerrc`. Properties: `applicationsPath`, `showDisclaimer`, `showNotifications`, `updateFrequency`, `customUpdateDays`, `manageIconSize`, `watchDownloads` (controls both daemon + dashboard download detection), `showInstallBox`, `accentBorders` (persisted; calls `m_config->sync()` immediately on set). Setter validates path via `QDir::mkpath()`, emits `applicationsPathError(msg)` on failure |
| `GitHubReleaseChecker` | `githubreleasechecker.h/.cpp` | Parses `gh-releases-zsync\|owner\|repo\|...` update info, hits GitHub Releases API, emits `updateAvailable(newVersion, zsyncUrl)`, `upToDate()`, `networkFailed()`, or `failed()`. Used by both `UpdateDaemon` and `AppImageUpdateManager` — single source of truth for GitHub update logic |
| `UpdateDaemon` | `updatedaemon.h/.cpp` | Background update checker. Launched via `--daemon` CLI flag (autostart desktop file installs to `$KDE_INSTALL_AUTOSTARTDIR`). Persistent tray entry (`KStatusNotifierItem`) with "Check Now" + "Open Dashboard" actions. Scans `applicationsPath` hourly (interval via `intervalForFrequency`); timer starts only when `updateFrequency != 0` — "Never" (0) leaves the timer stopped. When frequency changes at runtime: 0 → `stop()`; non-zero → `setInterval()` + `start()`. Uses `GitHubReleaseChecker` + parallelised cold-cache `AppImageReader` reads, fires `KNotification` on update found. Owns a `DownloadWatcher` for `~/Downloads`. Registers D-Bus name `io.github.appimagemanager.Daemon` on start so the dashboard can skip duplicate download notifications. Uses `Qt6::Network` |

### GUI (`src/gui/`) — Qt Quick dependency

| Class | Files | Role |
|-------|-------|------|
| `AppImageBackend` | `appimagebackend.h/.cpp` | QML context property `backend`. Owns `AppImageInfo` + all mutable install state. Exposes metadata + ops as Q_PROPERTYs and slots. Holds `CorpseModel`. Emits `uninstallFinished` |
| `AppImageIconProvider` | `appimageiconprovider.h/.cpp` | `QQuickImageProvider` for `image://appimage/<id>`. Thread-safe via split locks: `m_iconsLock` (`QReadWriteLock`) guards the icon byte map — shared reads in `requestPixmap`, exclusive write in `setIconData`; `m_cacheMutex` (`QMutex`) guards the rendered-pixmap `QCache` (needed because `QCache::object` mutates LRU state even on read). SVG/PNG rendering happens lock-free between the two lock scopes. Caches rendered pixmaps keyed by `(id, size)`. Key `"icon"` = manage window single icon; key = `qHash(path)` decimal string = dashboard per-file icon |
| `CorpseModel` | `corpsemodel.h/.cpp` | `QAbstractListModel` for leftover config/cache dirs. Roles: `filePath`, `fileSize`, `isChecked`. **No `QML_ELEMENT`** — Qt 6.11 constexpr metaobject bug. Exposed only via `AppImageBackend::corpseModel` Q_PROPERTY. `checkedRole` Q_PROPERTY replaces enum access from QML |
| `AppImageWindow` | `appimagewindow.h/.cpp` | One `QQuickWindow` per AppImage path. Deduplicates via `static QHash<QString, AppImageWindow*> s_instances`. Re-opening same path raises existing window |
| `DownloadWatcher` | `downloadwatcher.h/.cpp` | Shared `~/Downloads` watcher. `KDirWatch` in `WatchFiles` mode + settle pass (poll `size`/`mtime` until stable) so we never notify mid-download. Emits `appImageFound(path, displayName)` once per file. Used by both `UpdateDaemon` and `AppImageListModel`; the dashboard suppresses its own listener while the daemon owns the well-known D-Bus name. `isSandboxed()` returns `true` when `isRunningInSandbox()` detected at construction time (env vars `FLATPAK_ID` / `SNAP`); `setEnabled(true)` is a no-op in that case — KDirWatch cannot watch paths outside the sandbox |
| `AppImageUpdateManager` | `appimageupdate.h/.cpp` | Batched update checker + downloader. `checkForUpdates(items)` — per item, fetches `.zsync` header, compares `SHA-1` (calculated off-thread via `QtConcurrent`), emits `updateFound/upToDate`. `downloadUpdate(path, zsyncUrl, ...)` — spawns `zsync2` subprocess; if `zsync2` is missing on `PATH`, falls back to a full HTTP download via the `URL:` header in the `.zsync` file. Both paths use an atomic-ish swap: `old → old.bak`, then `old.new → old`, then `QFile::remove(bak)` on success. If either rename fails, restores `.bak` to original and emits failure. A leftover `.bak` after a crash = mid-swap interruption, recoverable manually. Signals: `checkingChanged`, `checkFinished(found, failures)`, `updateFound`, `downloadProgress`, `downloadFinished` |
| `AppImageListModel` | `appimagelistmodel.h/.cpp` | Dashboard list model. Roles: `filePath`, `displayName`, `cleanName`, `appName`, `version`, `iconSource`, `hasDesktopLink`, `metadataLoaded`, `appSize`, `formattedSize`, `addedDate`, `categories`, `comment`, `description`, `developerName`, `homepage`, `updateAvailable`, `updateVersion`, `isUpdating`, `updateProgress`, `isSelected`. Properties: `scanning`, `checkingUpdates`, `selectionMode`, `selectedCount`, `pendingLoads` (int — live count of in-flight metadata futures; QML uses this for a "Loading N…" label), `downloadWatcherSandboxed` (bool `CONSTANT` — true when `FLATPAK_ID`/`SNAP` env present). Invokables: `scan`, `refresh`, `toggleDesktopLink`, `requestRemoveAt`, `requestLaunch`, `checkForUpdates`, `downloadUpdate`, `setSelected`, `selectAll`, `clearSelection`, `selectedPaths`, `trashSelected`, `itemData(row)` (bulk role snapshot for QML). Watches `applicationsPath` via `KDirWatch` (`WatchFiles`) and routes `created/deleted/dirty` straight to incremental inserts/removes — no debounce, no full re-scan. Reader workers dispatch through `m_readerPool` (bounded to `qBound(1, QThread::idealThreadCount() / 2, 4)`) to prevent thread storms on large libraries. Holds an `AppImageUpdateManager` and a `DownloadWatcher`. `m_generation` invalidates stale futures on `refresh()`; `m_pendingLoads` drives both the `scanning` flag and the `pendingLoads` property |
| `AppImageSortFilterModel` | `appimagesortfiltermodel.h/.cpp` | Proxy over `AppImageListModel`. Sort: `SortByName=0`, `SortBySize=1`, `SortByCategory=5`, `SortByDate=9`. Filter: case-insensitive text match on `cleanName` + `appName`. `lessThan` reads typed values via `AppImageListModel::sortKey()` to skip QVariant role dispatch. Uses Qt 6.9 `beginFilterChange/endFilterChange` where applicable |
| `DashboardWindow` | `dashboardwindow.h/.cpp` | Singleton dashboard host. Creates its own `QQmlApplicationEngine`. Holds `m_listModel`, `m_proxyModel`, `m_uninstallBackend`, `m_storageBackend`. `createBackend(path, withCorpses)` — storage backend passes `false` (skips corpse scan) |

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
| `ManageWindow.qml` | `ApplicationWindow` (fixed 26×22 gridUnits) | Context prop: `backend` (AppImageBackend). Drag-and-drop install, inline `UninstallDialog` |
| `DashboardWindow.qml` | `Kirigami.ApplicationWindow` | Context props: `listModel`, `proxyModel`, `dashboardController`. Sort/filter/search, selection mode, update checks. Master list fills full window when no selection. Details pane: icon, heading (`elide: ElideRight`), chips (`Flow` — wraps when narrow), description (`ScrollView` vertical-only, `Text.StyledText`). Hosts inline `UpdateDialog`. `readonly property bool hasSelection: listView.currentIndex >= 0` cached on the `RowLayout` drives left-pane width/fill bindings — avoids two inline expression evaluations on every selection change. "Loading N…" label below progress bar reads `listModel.pendingLoads` |
| `UninstallDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Corpse checklist + total size + confirm. Sets `backend = null` on close. Used in both ManageWindow and DashboardWindow |
| `StorageDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Shows AppImage file + related dirs. Signal: `openInFileManager(path)`. No corpse scan |
| `UpdateDialog.qml` | `Kirigami.Dialog` | Props: `appName`, `currentVersion`, `newVersion`, `proxyRow`. Signal: `downloadRequested()`. Confirms before kicking off `AppImageUpdateManager::downloadUpdate()` |
| `SettingsDialog.qml` | `Kirigami.Dialog` | Uses `AppSettings` singleton. Path field + folder picker. Toggles: manage icon size, show install drag box, show security disclaimer, show notifications, notify when AppImage downloaded (`watchDownloads`), update frequency, accent borders. `Kirigami.InlineMessage` below the `watchDownloads` toggle — visible when `listModel.downloadWatcherSandboxed` is true — informs user that download watching is unavailable in sandboxed environments |

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
       └─ backend.installAppImage()
            └─ AppImageManager::installAppImage()
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

### Uninstall flow

```
user clicks Remove (single) or trashSelected (bulk)
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
- `DashboardWindow::m_uninstallBackend` / `m_storageBackend`: use `deleteLater()` when replacing, never raw `delete` — in-flight `QFutureWatcher` lambdas capture `this`.
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
- `StorageDialog` backend is created with `withCorpses = false` — it never reads `corpseModel`.
- Use `listModel.itemData(row)` for bulk role reads; per-role `.data()` round-trips dominate the detail pane otherwise.

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
9. Dashboard size chip → `StorageDialog` opens. Verify no corpse-scan log output:
   `QT_LOGGING_RULES="appimagemanager=true" appimagemanager --dashboard`
10. Dashboard delete button → `UninstallDialog` → list refreshes after trash.
11. Dashboard selection mode → select several rows → "Move to trash" → all rows trashed in one batch.
12. Dashboard "Check for updates" → progress bar; if any update found, `UpdateDialog` opens → confirm → download progresses; both `zsync2` present and absent (rename it temporarily) should succeed.
13. Settings → browse button opens folder picker → valid path saved → invalid path shows error.
14. Settings → disclaimer toggle persists across restart.
15. Select an AppImage with AppStream metadata (e.g. ProtonUp-Qt) → description text visible below chips in details pane; vertical scroll works if long; no horizontal scrollbar.
16. Settings → "Notify when an AppImage is downloaded" on → copy a `.AppImage` into `~/Downloads` → KDE notification fires after settle pass → click "Manage" → ManageWindow opens for that file.
17. Toggle download notification off → copy another AppImage to `~/Downloads` → no notification fires.
18. Start daemon (`appimagemanager --daemon`) → tray entry appears with "Check Now" + "Open Dashboard". Open dashboard while daemon runs → drop a file in `~/Downloads` → only one notification fires (daemon wins).
19. AboutDialog: open dialog → disable network (`nmcli networking off`) → reopen About → wait 8 s → spinner hides, fallback contributors shown, no SIGABRT. Re-enable network, reopen → real contributors load.
20. Sandbox: run `FLATPAK_ID=test appimagemanager`, open Settings → "Download watching is unavailable in sandboxed environments." InlineMessage visible below toggle. Copy a `.AppImage` to `~/Downloads` → no notification fires.
21. Update: trigger update on a test AppImage → verify `.bak` file created mid-swap; on success `.bak` removed. Kill process during swap → `.bak` present afterward for manual recovery.
