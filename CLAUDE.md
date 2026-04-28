# CLAUDE.md

This file is the authoritative reference for working on AppImage Manager.

## Project Overview

**AppImage Manager** is a lightweight KDE Plasma 6 utility for installing, managing, and removing AppImage files. It integrates with Dolphin via a right-click context menu plugin and provides a standalone dashboard for browsing all installed AppImages.

**Distribution target:** KDE Store and AUR.  
**Philosophy:** Simplicity and efficiency. No unnecessary abstractions. Every feature must justify its existence.  
**Stack:** C++20, Qt6, KDE Frameworks 6, Kirigami (Plasma 6 era), QML.  
**License:** GPL-2.0-or-later.

### Roadmap (planned, not yet implemented)
- **Update checking** — detect newer versions of installed AppImages (AppImageUpdate / zsync).
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

Build output: `build/<presetName>/`. No test suite — validation is manual (see Testing).

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
| `AppImageInfo` | `appimageinfo.h` | Value struct: `originalName`, `cleanName`, `appId`, `appName`, `version`, `categories`, `execArgs`, `fileSize`, `iconData`, `iconExt`, `isValid` |
| `AppImageReader` | `appimagereader.h/.cpp` | **BLOCKING** extractor. Preferred: `libappimage` (in-process SquashFS). Fallback: `squashfuse` subprocess + `fusermount3`. Detects Type 1 vs 2 via ELF magic at byte offset 8. **Always call via `QtConcurrent::run`** |
| `AppImageManager` | `appimagemanager.h/.cpp` | File operations: `installAppImage()` (KIO::CopyJob + chmod +x), `createDesktopLink()`, `removeDesktopLink()`, `isDesktopLinkEnabled()`, `findCorpses()` (blocking), `removeItems()` (KIO::trash). Also `rebuildSycoca()` |
| `AppSettings` | `appsettings.h/.cpp` | QML singleton (`AppSettings`). KSharedConfig → `appimagemanagerrc`. Properties: `applicationsPath` (default `~/Applications`), `showDisclaimer`. Setter validates via `QDir::mkpath()`, emits `applicationsPathError(msg)` on failure |

### GUI (`src/gui/`) — Qt Quick dependency

| Class | Files | Role |
|-------|-------|------|
| `AppImageBackend` | `appimagebackend.h/.cpp` | QML context property `backend`. Owns `AppImageInfo` + all mutable install state. Exposes metadata + ops as Q_PROPERTYs and slots. Holds `CorpseModel`. Emits `uninstallFinished` |
| `AppImageIconProvider` | `appimageiconprovider.h/.cpp` | `QQuickImageProvider` for `image://appimage/<id>`. Thread-safe via `QReadWriteLock`. Key "icon" = manage window single icon; key = `qHash(path)` decimal string = dashboard per-file icon |
| `CorpseModel` | `corpsemodel.h/.cpp` | `QAbstractListModel` for leftover config/cache dirs. Roles: `filePath`, `fileSize`, `isChecked`. **No `QML_ELEMENT`** — Qt 6.11 constexpr metaobject bug. Exposed only via `AppImageBackend::corpseModel` Q_PROPERTY. `checkedRole` Q_PROPERTY replaces enum access from QML |
| `AppImageWindow` | `appimagewindow.h/.cpp` | One `QQuickWindow` per AppImage path. Deduplicates via `static QHash<QString, AppImageWindow*> s_instances`. Re-opening same path raises existing window |
| `AppImageListModel` | `appimagelistmodel.h/.cpp` | Dashboard list model. Roles: `filePath`, `displayName`, `cleanName`, `appName`, `version`, `iconSource`, `hasDesktopLink`, `metadataLoaded`, `appSize`, `formattedSize`, `addedDate`. Watches dir via `QFileSystemWatcher` + 500ms debounce `QTimer`. `m_pendingLoads` counter tracks async futures; `scanning` stays true until all finish |
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
| `DashboardWindow.qml` | `Kirigami.ApplicationWindow` | Context props: `listModel`, `proxyModel`, `dashboardController`. Sort/filter/search |
| `UninstallDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Corpse checklist + total size + confirm. Sets `backend = null` on close. Used in both ManageWindow and DashboardWindow |
| `StorageDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Shows AppImage file + related dirs. Signal: `openInFileManager(path)`. No corpse scan |
| `SettingsDialog.qml` | `Kirigami.Dialog` | Uses `AppSettings` singleton. Path field + browse button (`FolderDialog`) + error `InlineMessage` |

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
  └─ QFileSystemWatcher watches applicationsPath
       └─ directoryChanged → 500ms debounce → refresh() [clears + re-scans]
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
| **`beginFilterChange`/`endFilterChange`** | Added in Qt 6.9. Project minimum is Qt 6.6. Use `invalidateFilter()` |
| **`CorpseModel` QML_ELEMENT** | Do not add `QML_ELEMENT` — triggers Qt 6.11 constexpr metaobject `static_assert` |
| **KDBusService mode** | Uses `Multiple` so Dolphin can open several AppImages simultaneously. Do not change to `Unique` |

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
