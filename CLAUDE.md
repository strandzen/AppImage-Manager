# CLAUDE.md

This file is the authoritative reference for working on AppImage Manager.

## Project Overview

> **Disclaimer:** This project and its author are in no way affiliated with KDE or the KDE e.V. organization. While the project's goal is to provide a native-like experience on the Plasma desktop, it is an independent, community-driven project authored by the user. It should not pretend to be an official KDE product (e.g., `org.kde.*`).

**AppImage Manager** is a lightweight KDE Plasma 6 utility for installing, managing, and removing AppImage files. It integrates with Dolphin via a right-click context menu plugin and provides a standalone dashboard for browsing all installed AppImages.

**Distribution target:** KDE Store and AUR.  
**Philosophy:** Simplicity and efficiency. No unnecessary abstractions. Every feature must justify its existence.  
**Stack:** C++20, Qt6, KDE Frameworks 6, Kirigami (Plasma 6 era), QML.  
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
```

Build output: `build/<presetName>/`. Unit tests in [tests/](tests/) (run via `ctest --test-dir build/dev --output-on-failure`); the manual checklist below still applies for UI/integration paths.

---

## Architecture — Build Targets

| Target | Type | Description |
|--------|------|-------------|
| `appimagemanager_core` | Object library | Pure C++, zero Qt Quick dependency. Reader, manager, info struct, logging |
| `appimagemanager_qml` | Shared lib + QML module `appimagemanager` | All GUI backend classes + QML files |
| `appimagemanager` (plugin `.so`) | `KAbstractFileItemActionPlugin` | Dolphin right-click plugin, auto-discovered via JSON metadata |
| `appimagemanager_bin` → `appimagemanager` | Executable | Service menu launcher + CLI. No args → dashboard. With file arg → manage window |

Both the plugin `.so` and the binary link `appimagemanager_qml` — identical logic, two entry points.

---

## Class Map

### Core (`src/core/`) — no Qt Quick

| Class | Files | Role |
|-------|-------|------|
| `AppImageInfo` | `appimageinfo.h` | Value struct: `originalName`, `cleanName`, `appId`, `appName`, `version`, `categories`, `comment`, `description`, `execArgs`, `fileSize`, `iconData`, `iconExt`, `updateInfo`, `isValid`. `description` = AppStream XML content; falls back to `comment` (`.desktop Comment=`) if XML absent |
| `AppImageReader` | `appimagereader.h/.cpp` | **BLOCKING** extractor. Requires `libappimage` (in-process SquashFS, no FUSE/subprocess). Reads `.desktop` via `KDesktopFile`, then AppStream XML from `usr/share/metainfo/*.appdata.xml` or `usr/share/appdata/*.metainfo.xml` via `QXmlStreamReader`. **Always call via `QtConcurrent::run`** |
| `AppImageCache` | `appimagecache.h/.cpp` | Thread-safe on-disk cache (SQLite via `Qt6::Sql`, WAL mode, per-thread connection; DB path `$XDG_DATA_HOME/AppImageManager/cache.db`; one row per AppImage keyed by `MD5(path)`). **`kCacheVersion = 4`** — schema migrated from QSettings INI in v4; adds `developer_name` / `homepage`. Rows with `cache_version != 4` or stale `mtime` are returned as invalid (forces a cold re-read). |
| `AppImageManager` | `appimagemanager.h/.cpp` | File operations: `installAppImage()` (KIO::move when source is under `~/Downloads`, KIO::copy otherwise — preserves originals on USB sticks / project dirs; chmod +x on result), `createDesktopLink()`, `removeDesktopLink()`, `isDesktopLinkEnabled()`, `findCorpses()` (blocking), `removeItems()` (KIO::trash). Also `rebuildSycoca()` (called automatically from createDesktopLink / removeDesktopLink) |
| `AppSettings` | `appsettings.h/.cpp` | QML singleton (`AppSettings`). KSharedConfig → `appimagemanagerrc`. Properties: `applicationsPath`, `showDisclaimer`, `showNotifications`, `updateFrequency`, `customUpdateDays`, `manageIconSize`, `watchDownloads` (controls both daemon + dashboard download detection), `showInstallBox`. Setter validates path via `QDir::mkpath()`, emits `applicationsPathError(msg)` on failure |
| `GitHubReleaseChecker` | `githubreleasechecker.h/.cpp` | Parses `gh-releases-zsync\|owner\|repo\|...` update info, hits GitHub Releases API, emits `updateAvailable(newVersion, zsyncUrl)`, `upToDate()`, or `failed()`. Used by both `UpdateDaemon` and `AppImageListModel` — single source of truth for GitHub update logic |
| `UpdateDaemon` | `updatedaemon.h/.cpp` | Background update checker. Launched via `--daemon` CLI flag (autostart desktop file installs to `$KDE_INSTALL_AUTOSTARTDIR`). Scans `applicationsPath` hourly, uses `GitHubReleaseChecker` for GitHub updates, fires `KNotification` on update found. Also watches `~/Downloads` for new AppImages. Registers D-Bus name `io.github.appimagemanager.Daemon` on start so the dashboard can skip duplicate download notifications. Uses `Qt6::Network` |

### GUI (`src/gui/`) — Qt Quick dependency

| Class | Files | Role |
|-------|-------|------|
| `AppImageBackend` | `appimagebackend.h/.cpp` | QML context property `backend`. Owns `AppImageInfo` + all mutable install state. Exposes metadata + ops as Q_PROPERTYs and slots. Holds `CorpseModel`. Emits `uninstallFinished` |
| `AppImageIconProvider` | `appimageiconprovider.h/.cpp` | `QQuickImageProvider` for `image://appimage/<id>`. Thread-safe via `QReadWriteLock`. Key "icon" = manage window single icon; key = `qHash(path)` decimal string = dashboard per-file icon |
| `CorpseModel` | `corpsemodel.h/.cpp` | `QAbstractListModel` for leftover config/cache dirs. Roles: `filePath`, `fileSize`, `isChecked`. **No `QML_ELEMENT`** — Qt 6.11 constexpr metaobject bug. Exposed only via `AppImageBackend::corpseModel` Q_PROPERTY. `checkedRole` Q_PROPERTY replaces enum access from QML |
| `AppImageWindow` | `appimagewindow.h/.cpp` | One `QQuickWindow` per AppImage path. Deduplicates via `static QHash<QString, AppImageWindow*> s_instances`. Re-opening same path raises existing window |
| `AppImageListModel` | `appimagelistmodel.h/.cpp` | Dashboard list model. Roles: `filePath`, `displayName`, `cleanName`, `appName`, `version`, `iconSource`, `hasDesktopLink`, `metadataLoaded`, `appSize`, `formattedSize`, `addedDate`, `categories`, `comment`, `description`. Watches `applicationsPath` via `QFileSystemWatcher` + 500ms debounce. When `watchDownloads` is on, also watches `~/Downloads` — `directoryChanged` routes by path: downloads dir → `checkNewDownloads()` (KNotification + "Manage" action, skipped if daemon D-Bus name is registered), other → refresh. `m_pendingLoads` tracks async futures; `scanning` stays true until all finish. Private helpers: `findRowByPath(path)` — returns row index or -1; `checkZsyncUpdate(row)` — handles `zsync\|` header fetch; `sendError(parent, title, text)` — static KNotification helper |
| `AppImageSortFilterModel` | `appimagesortfiltermodel.h/.cpp` | Proxy over `AppImageListModel`. Sort: name / size / date (SortRole enum). Filter: case-insensitive text match on `cleanName` + `appName`. Uses `invalidateFilter()` — **not** `beginFilterChange/endFilterChange` (Qt 6.9+ only) |
| `DashboardWindow` | `dashboardwindow.h/.cpp` | Singleton dashboard host. Creates its own `QQmlApplicationEngine`. Holds `m_listModel`, `m_proxyModel`, `m_uninstallBackend`, `m_storageBackend`. `createBackend(path, withCorpses)` — storage backend passes `false` (skips corpse scan) |

### Plugin (`src/plugin/`)

| Class | Files | Role |
|-------|-------|------|
| `AppImageActionPlugin` | `appimageactionplugin.h/.cpp` | Dolphin plugin. `actions()` returns "Manage AppImage" `QAction` that launches `appimagemanager <path>` via `QProcess::startDetached` |

---

## QML File Map

| File | Type | Context / Properties |
|------|------|----------------------|
| `ManageWindow.qml` | `ApplicationWindow` (fixed 26×22 gridUnits) | Context prop: `backend` (AppImageBackend). Drag-and-drop install, inline `UninstallDialog` |
| `DashboardWindow.qml` | `Kirigami.ApplicationWindow` | Context props: `listModel`, `proxyModel`, `dashboardController`. Sort/filter/search. Details pane: icon, heading (`elide: ElideRight`), chips (`Flow` — wraps when narrow), description (`ScrollView` vertical-only, `Text.StyledText`) |
| `UninstallDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Corpse checklist + total size + confirm. Sets `backend = null` on close. Used in both ManageWindow and DashboardWindow |
| `StorageDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Shows AppImage file + related dirs. Signal: `openInFileManager(path)`. No corpse scan |
| `SettingsDialog.qml` | `Kirigami.Dialog` | Uses `AppSettings` singleton. Path field + folder picker. Toggles: manage icon size, show install drag box, show security disclaimer, show notifications, notify when AppImage downloaded (watchDownloads), update frequency |

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
  └─ ManageWindow.qml                BusyIndicator until metadataLoaded; then icon + info shown
  └─ user drags icon to folder
       └─ backend.installAppImage()
            └─ AppImageManager::installAppImage() → KIO::CopyJob (move to ~/Applications + chmod +x)
            └─ onInstallJobFinished → isInstalled = true → folder column fades out → icon stays centered
```

### Dashboard: initial scan

```
DashboardWindow::setupAndShow()
  └─ AppImageListModel::scan()
       └─ QDir(applicationsPath).entryInfoList(*.AppImage / *.appimage)
       └─ beginInsertRows → items added immediately with placeholder info (metadataLoaded = false)
       └─ for each item: loadMetadataForRow(i)
            └─ QtConcurrent::run(AppImageReader(path).read())
            └─ QFutureWatcher::finished
                 → m_items[row].info = result; metadataLoaded = true
                 → dataChanged(row, row)
                 → if (--m_pendingLoads == 0) scanning = false
  └─ QFileSystemWatcher watches applicationsPath (and ~/Downloads when watchDownloads is on)
       └─ directoryChanged(path)
            ├─ path == ~/Downloads → checkNewDownloads() → KNotification "Manage"
            └─ other path → 500ms debounce → refresh() [clears + re-scans]
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
       └─ emits uninstallFinished → ManageWindow closes / DashboardWindow list refreshes
```

---

## Key Conventions

### Threading
- `AppImageReader::read()` and `AppImageManager::findCorpses()` are **always blocking** — call only via `QtConcurrent::run`.
- All async results marshal back to main thread via `QFutureWatcher::finished`.
- Never touch `m_items` or `m_scanning` from worker threads.

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
- Dialogs: take a `backend` property, set it to `null` in `onClosed`.
- `UninstallDialog` is a dialog, not a window — it is instantiated inline in its parent.
- `StorageDialog` backend is created with `withCorpses = false` — it never reads `corpseModel`.

### Shared helpers (`appimageinfo.h`)

Three `inline` utilities defined at file scope in `appimageinfo.h` (included by everything):

- `normalizeVersion(v)` — strips leading `v`/`V` from version strings.
- `isNewerVersion(remote, local)` — compares dot-split version tuples; returns true if remote > local.
- `kAppImageFilters()` — returns `{ "*.AppImage", "*.appimage" }` as a static `QStringList`. Use everywhere instead of inline literals.

### D-Bus service name

The daemon registers `io.github.appimagemanager.Daemon` on the session bus when started. `AppImageListModel::checkNewDownloads()` checks for this name via `QDBusConnection::sessionBus().interface()->isServiceRegistered(...)` and silently returns if the daemon is running — prevents duplicate download notifications when both the daemon and dashboard are active simultaneously.

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
| **KDBusService mode** | Uses `Multiple` so Dolphin can open several AppImages simultaneously. Do not change to `Unique` |
| **`AppImageCache` version** | Cache is versioned (`kCacheVersion = 4`, SQLite-backed). Adding new fields to `AppImageInfo` that must be persisted requires bumping `kCacheVersion`, adding a column to the `CREATE TABLE metadata` schema, and adding the field to both `load()` and `store()`. Forgetting any of the three silently returns empty strings for the new field. |
| **`QFileSystemWatcher::directoryChanged` routing** | Signal provides the changed path. `AppImageListModel` uses `[this](const QString &p)` and routes: `p == DownloadLocation` → `checkNewDownloads()`, else → `m_refreshTimer.start()`. Do not revert to a no-arg lambda or downloads will trigger a full refresh instead of a notification. |
| **`QLatin1String` and multi-byte UTF-8** | `QLatin1String("• ")` misinterprets the bullet (U+2022, 3 UTF-8 bytes) as 3 Latin-1 chars. Always use `QStringLiteral("• ")` for non-ASCII string literals. |

---

## Testing Checklist (Manual)

After any change, build and verify:

1. `cmake --build --preset dev` → zero errors.
2. `sudo cmake --install build/dev` + `kquitapp6 dolphin && dolphin &`.
3. Right-click an uninstalled `.AppImage` → "Manage AppImage" → window opens at correct fixed size.
4. Drag icon to folder → install animates → folder column fades → icon centers.
5. Click installed icon → app launches.
6. Click Remove → `UninstallDialog` opens inline → corpse list populates → trash works → window closes.
7. `appimagemanager` (no args) → DashboardWindow opens, scans, shows list with icons.
8. Dashboard search → filters live. Sort by name/size/date → list re-orders with animation.
9. Dashboard size chip → `StorageDialog` opens. Verify no corpse-scan log output:
   `QT_LOGGING_RULES="appimagemanager=true" appimagemanager --dashboard`
10. Dashboard delete button → `UninstallDialog` → list refreshes after trash.
11. Settings → browse button opens folder picker → valid path saved → invalid path shows error.
12. Settings → disclaimer toggle persists across restart.
13. Select an AppImage with AppStream metadata (e.g. ProtonUp-Qt) → description text visible below chips in details pane; vertical scroll works if long; no horizontal scrollbar.
14. Settings → "Notify when an AppImage is downloaded" on → copy a `.AppImage` into `~/Downloads` → KDE notification fires → click "Manage" → ManageWindow opens for that file.
15. Toggle download notification off → copy another AppImage to `~/Downloads` → no notification fires.
