# CLAUDE.md

This file is the authoritative reference for working on AppImage Manager.

## Project Overview

> **Disclaimer:** This project and its author are in no way affiliated with KDE or the KDE e.V. organization. While the project's goal is to provide a native-like experience on the Plasma desktop, it is an independent, community-driven project authored by the user. It should not pretend to be an official KDE product (e.g., `org.kde.*`).

**AppImage Manager** is a lightweight KDE Plasma 6 utility for installing, managing, updating, and removing AppImage files. It integrates with Dolphin via a right-click context menu plugin and provides a standalone dashboard for browsing, batch-managing, and updating all installed AppImages.

**Distribution target:** KDE Store and AUR.  
**Philosophy:** Simplicity, efficiency, and complete source agnosticism. No unnecessary abstractions. Every feature must justify its existence. The utility strives to remain entirely agnostic to where AppImages are sourced from, supporting custom repository configurations natively.  
**Stack:** C++20, Qt 6.9, KDE Frameworks 6.7, KirigamiAddons, Kirigami (Plasma 6 era), QML. Project version `2.1.0`.  
**License:** GPL-2.0-or-later.

### AI Assistant Guidelines

- **Plan Backups:** Always "backup" every big change or implementation plan as a `.md` file inside the `markdown/plans/` directory of the project.
- **Ask, don't guess.** The user prefers too many questions over too few. When a choice is the user's (UX, scope, what to remove), ask before acting.
- **Verify APIs, don't guess them.** Check Qt/KF6/Kirigami names against the installed headers or QML modules (`/usr/include/qt6`, `/usr/include/KF6`, `/usr/lib/qt6/qml`) before using them. Several pitfalls below came from plausible-looking APIs that do not exist.
- **Writing style:** in `README.md`, do not use em dashes (—). Use regular dashes (-) instead.
- **Todo.md maintenance:** open tasks in [Todo.md](Todo.md) are grouped by priority (High / Medium / Low), not feature area. When adding a task, ask which priority it belongs in rather than guessing. When a task is done, move it to the **Completed** section (grouped by feature area there) instead of marking it done in place.
- **CHANGELOG.md:** add a line under `## Unreleased` for every user-visible change.

#### Behavioral principles (derived from Karpathy's LLM coding observations)

**1. Think before coding.** State assumptions explicitly before implementing. If multiple interpretations exist, surface them — don't pick silently. If something is unclear, stop and ask. Push back when a simpler approach exists.

**2. Simplicity first.** Minimum code that solves the problem. No features beyond what was asked. No abstractions for single-use code. No speculative flexibility or configurability. If it could be 50 lines, don't write 200.

**3. Surgical changes.** Touch only what the request requires. Don't improve adjacent code, comments, or formatting. Match existing style even if you'd do it differently. Mention unrelated dead code — don't delete it. Remove only imports/variables/functions that *your* changes made unused.

**4. Goal-driven execution.** Transform tasks into verifiable goals before starting. For multi-step tasks, state a brief plan with per-step success criteria. Loop until verified — don't hand back unverified work.

### Roadmap

Planned work lives in [Todo.md](Todo.md).

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

# Install as a pacman package from this checkout (replaces the old install.sh)
cd packaging && makepkg -si

# Release build
cmake --preset release
cmake --build --preset release
sudo cmake --install build/release

# Clean rebuild
rm -rf build/ && cmake --preset dev && cmake --build --preset dev

# Run unit tests
ctest --test-dir build/dev --output-on-failure

# Syntax-check a single QML file headlessly (124 = ran fine until timeout, 2 = real error).
# Confirms syntax/imports only, not runtime behaviour — still test live.
QT_QPA_PLATFORM=offscreen QML_IMPORT_PATH=build/dev/src timeout 5 qml6 qml/<File>.qml; echo $?
```

**When releasing:** bump `VERSION` in the top-level `CMakeLists.txt` (`project(AppImageManager VERSION X.Y.Z ...)`). This is the single source of truth — `version.h` is generated from it and `AppSettings::appVersion()` reads it at runtime. Also update the version string in this file, move `CHANGELOG.md`'s `## Unreleased` entries under the new version, and add a `<release>` entry to `data/appimagemanager.metainfo.xml`. For the AUR, bump `pkgver` in `packaging/aur/PKGBUILD`, update `sha256sums`, and regenerate `.SRCINFO` (`makepkg --printsrcinfo > .SRCINFO`). Forgetting any of these causes the About dialog to display the wrong version or the KDE Store validator to reject the submission.

Build output: `build/<presetName>/`. Unit tests in [tests/](tests/) cover `appimageinfo` helpers, `AppImageReader` cleanName, `AppImageCache` round-trip, `AppImageListModel` watcher behavior, and update sources and launch environment (`tst_updatesource`: input auto-detection, effective update info, config round-trip incl. the legacy `Repo` key, release asset pick, environment parsing, Content-Type guard). The manual checklist below still applies for UI/integration paths.

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
| [packaging/](packaging/) | `PKGBUILD` (local `makepkg -si`), `aur/` (release, published), `aur-git/` (latest commit), `build-appimage.sh` |

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
| `AppImageCache` | `appimagecache.h/.cpp` | Thread-safe on-disk cache (SQLite via `Qt6::Sql`, WAL mode, per-thread connection; DB path `$XDG_DATA_HOME/AppImageManager/cache.db`; one row per AppImage keyed by `MD5(path)`). **`kCacheVersion = 5`** — schema migrated from QSettings INI in v4 (adds `developer_name` / `homepage`); v5 forces a re-read after the appId fix. Rows with `cache_version != 5` or stale `mtime` are returned as invalid (forces a cold re-read) |
| `AppImageManager` | `appimagemanager.h/.cpp` | File operations: `installAppImage()` (KIO::move when source is under `~/Downloads`, KIO::copy otherwise — preserves originals on USB sticks / project dirs; chmod +x on result), `createDesktopLink()` (reuses an entry the app wrote, keeping its other keys), `removeDesktopLink()` (every entry that starts the AppImage; ours deleted, others trashed), `isDesktopLinkEnabled()` (any visible entry whose `Exec` starts the AppImage, via a directory-mtime-cached index of `~/.local/share/applications`), `removeDuplicateDesktopLinks()` (drops our `appimage_*` entries when another entry starts the same AppImage; run on every metadata load), `findCorpses()` (blocking), `removeItems()` (KIO::trash). `rebuildSycoca()` + `update-desktop-database` + `update-icon-caches` called from `createDesktopLink`/`removeDesktopLink` — covers both Plasma's service catalog and XDG-spec caches for non-Plasma launchers. `fileKey(info)` derives icon and fallback desktop filenames: the cleanName slug plus a 6-char `qHash(originalName)` suffix (prevents collisions between two AppImages with the same display name) |
| `AppSettings` | `appsettings.h/.cpp` | QML singleton (`AppSettings`). Delegates all reads/writes to the KConfig XT-generated `AppImageManagerSettings::self()` (schema: `appimagemanagersettings.kcfg`). Properties: `applicationsPath`, `showDisclaimer`, `firstRun`, `showNotifications`, `updateFrequency`, `customUpdateDays`, `watchDownloads`, `showInstallBox`, `verifySignatures`, `discoverEnabled`. Each setter calls `AppImageManagerSettings::self()->save()`. Setter validates path via `QDir::mkpath()`, emits `applicationsPathError(msg)` on failure |
| `ReleaseChecker` | `releasechecker.h/.cpp` | `check(filePath, embeddedUpdateInfo, currentVersion)`: applies the user's update source (`updatesource.h`) and handles `gh-releases-zsync\|owner\|repo\|tag\|pattern` (GitHub API `/releases/latest`, or `/releases` with pre-releases; Atom feed fallback), `forgejo-releases\|host\|owner\|repo` (Codeberg/Forgejo/Gitea `/api/v1/repos/o/r/releases[/latest]`, same JSON) and `direct\|url` (HEAD request; Content-Type must not be a web page; Last-Modified, else ETag, is the build marker shown as the version and stored as `installedTag`; without a stored marker the local file's mtime is compared). Emits one `checkFinished(Result, newVersion, url)`. `url` is a `.zsync` or `.AppImage` asset picked by `pickAsset()` (ported from Shelly: `.zsync` first, glob pattern, CPU architecture aliases matched as whole tokens (`Swarm` is not `arm`, `x86_64` is not `x86`), version-stripped name match against the local file; ambiguous = `Failed`). Version rule: remote tag == stored `installedTag` → up to date; unknown local version → update available; else `isNewerVersion`. `isNonBinaryContentType()` is shared with the download path. Used by both `UpdateDaemon` and `AppImageUpdateManager` |
| `UpdateSource` | `updatesource.h/.cpp` | Per-AppImage update source chosen by the user: `type` (`GitHub` "owner/repo", `Forgejo` "host/owner/repo", `Zsync` URL, `Url` direct download), `value`, `prerelease`, plus `installedTag` written after a successful update. Stored in `appimagemanagerrc` `[UpdateSources][<file name>]` as `Type`/`Value` (a legacy `Repo` key reads as GitHub), re-read on every load (dashboard writes, daemon reads). `parseUpdateSourceInput(input, forced)` auto-detects the kind; `effectiveUpdateInfo()` maps an override to `gh-releases-zsync\|`, `forgejo-releases\|`, `zsync\|` or `direct\|`; `isSupportedUpdateInfo()`, `isReleaseUpdateInfo()` (everything ReleaseChecker handles), `describeUpdateInfo()` ("Codeberg · o/r") |
| `launchenvironment` | `launchenvironment.h/.cpp` | Per-AppImage launch environment (`KEY=value` list) in `[Environment][<file name>]`. `parseEnvironment()` (one per line, `#` comments, invalid/duplicate lines reported), `launchWithEnvironment()` (used by the dashboard and manage window), `environmentExecPrefix()` (`env 'K=v' …` prepended to the `.desktop` `Exec=` by `createDesktopLink`) |
| `UpdateDaemon` | `updatedaemon.h/.cpp` | Background update checker. Launched via `--daemon` CLI flag (autostart desktop file installs to `$KDE_INSTALL_AUTOSTARTDIR`). Persistent tray entry (`KStatusNotifierItem`) with "Check Now" + "Open Dashboard" actions. Scans `applicationsPath` hourly (interval via `intervalForFrequency`); timer starts only when `updateFrequency != 0` — "Never" (0) leaves the timer stopped. When frequency changes at runtime: 0 → `stop()`; non-zero → `setInterval()` + `start()`. Uses `ReleaseChecker` (every release-type source: GitHub, Forgejo/Codeberg, direct URL; plain zsync stays dashboard-only) + parallelised cold-cache `AppImageReader` reads, passing the full path, fires `KNotification` on update found. Owns a `DownloadWatcher` for `~/Downloads`. Registers D-Bus name `io.github.appimagemanager.Daemon` on start so the dashboard can skip duplicate download notifications. Uses `Qt6::Network` |

### GUI (`src/gui/`) — Qt Quick dependency

| Class | Files | Role |
|-------|-------|------|
| `AppImageBackend` | `appimagebackend.h/.cpp` | QML context property `backend`. Owns `AppImageInfo` + all mutable install state. Exposes metadata + ops as Q_PROPERTYs and slots. Holds `CorpseModel`. Emits `uninstallFinished`. Install flow is split: `beginInstall()` runs `AppImageReader::verifySignature()` off-thread (if `AppSettings::verifySignatures` is on) and emits `signatureCheckReady(int state)` — QML inspects the `SigState` Q_ENUM value and either calls `doInstall()` directly or shows a warning dialog first. `SigState` enum (Unchecked=0, Valid=1, Invalid=2, Unsigned=3, GpgUnavailable=4) mirrors `SignatureState` and is registered as Q_ENUM for QML access |
| `AppImageIconStore` | `appimageiconstore.h/.cpp` | Process-wide singleton holding raw icon bytes extracted from AppImages. Shared across all QML engine instances (dashboard + manage windows) so multiple windows don't re-extract the same bytes. Thread-safe write (`setIconData`) via internal lock |
| `AppImageIconProvider` | `appimageiconprovider.h/.cpp` | `QQuickImageProvider` for `image://appimage/<id>`. Reads raw bytes from `AppImageIconStore`; each provider instance owns its own per-engine `QPixmap` render cache (`m_cacheMutex` guards the `QCache` — `QCache::object` mutates LRU state even on read). SVG/PNG rendering happens between the store read and cache write. Caches rendered pixmaps keyed by `(id, size)`. Key `"icon"` = manage window single icon; key = `qHash(path)` decimal string = dashboard per-file icon |
| `CorpseModel` | `corpsemodel.h/.cpp` | `QAbstractListModel` for leftover config/cache dirs. Roles: `filePath`, `fileSize`, `isChecked`. **No `QML_ELEMENT`** — Qt 6.11 constexpr metaobject bug. Exposed only via `AppImageBackend::corpseModel` Q_PROPERTY. `checkedRole` Q_PROPERTY replaces enum access from QML |
| `AppImageWindow` | `appimagewindow.h/.cpp` | One `QQuickWindow` per AppImage path. Deduplicates via `static QHash<QString, AppImageWindow*> s_instances`. Re-opening same path raises existing window |
| `DownloadWatcher` | `downloadwatcher.h/.cpp` | Shared `~/Downloads` watcher. `KDirWatch` in `WatchFiles` mode + settle pass (poll `size`/`mtime` until stable) so we never notify mid-download. Emits `appImageFound(path, displayName)` once per file. Used by both `UpdateDaemon` and `AppImageListModel`; the dashboard suppresses its own listener while the daemon owns the well-known D-Bus name. `isSandboxed()` returns `true` when `isRunningInSandbox()` detected at construction time (env vars `FLATPAK_ID` / `SNAP`); `setEnabled(true)` is a no-op in that case — KDirWatch cannot watch paths outside the sandbox |
| `AppImageUpdateManager` | `appimageupdate.h/.cpp` | Batched update checker + downloader. `checkForUpdates(items)` — replaces each item's update info with `effectiveUpdateInfo()` and drops unsupported ones; release-type items go through `ReleaseChecker`, plain `zsync\|` items fetch the `.zsync` header and compare `SHA-1` (calculated off-thread via `QtConcurrent`); emits `updateFound(path, version, url)`. Items are checked one at a time with a 30 s watchdog per item (`checkFinished(-1, 0)` when one hangs); `m_batch` is bumped per batch and on time-out so late replies are dropped. `downloadUpdate(path, url, ...)` — a `.zsync` URL spawns `zsync2` (or, when `zsync2` is missing on `PATH`, a full HTTP download via the `URL:` header in the `.zsync` file); any other URL is a direct `.AppImage` HTTP download (`downloadBinary`), aborted when the first response chunk has a web-page Content-Type. Both paths use an atomic-ish swap: `old → old.bak`, then `old.new → old`, then `QFile::remove(bak)` on success. If either rename fails, restores `.bak` to original and emits failure. A leftover `.bak` after a crash = mid-swap interruption, recoverable manually. Signals: `checkingChanged`, `checkFinished(found, failures)`, `updateFound`, `downloadProgress`, `downloadFinished` |
| `AppImageListModel` | `appimagelistmodel.h/.cpp` | Dashboard list model. Roles: `filePath`, `displayName`, `cleanName`, `appName`, `version`, `iconSource`, `hasDesktopLink`, `metadataLoaded`, `appSize`, `formattedSize`, `addedDate`, `categories`, `comment`, `description`, `developerName`, `homepage`, `updateAvailable`, `updateVersion`, `isUpdating`, `updateProgress`, `sectionCategory` (first category or "Other"), `sectionAdded` (Today / This Week / This Month / Earlier). `itemData()` also carries `environment` and `updateSourceLabel`. Properties: `scanning`, `checkingUpdates`, `updateCount` (int — rows with an update available; drives the "Installed (N)" tab and Update All), `downloadWatcherSandboxed` (bool `CONSTANT` — true when `FLATPAK_ID`/`SNAP` env present). Invokables: `scan`, `refresh`, `toggleDesktopLink`, `requestRemoveAt`, `requestLaunch`, `checkForUpdates`, `downloadUpdate`, `itemData(row)` (bulk role snapshot for QML), `updateSource(row)` / `setUpdateSource(row, input, type, prerelease)` / `describeUpdateSourceInput(input, type)` (per-AppImage update source; saving resets the row's update state, the user re-runs Check for Updates). `environment(row)` / `setEnvironment(row, text)` (launch environment; rewrites the app menu entry). On a successful release-type update it stores the installed tag via `saveInstalledTag()`. Watches `applicationsPath` via `KDirWatch` (`WatchFiles`) and routes `created/deleted/dirty` straight to incremental inserts/removes — no debounce, no full re-scan. Reader workers dispatch through `m_readerPool` (bounded to `qBound(1, QThread::idealThreadCount() / 2, 4)`) to prevent thread storms on large libraries. Holds an `AppImageUpdateManager` and a `DownloadWatcher`. `m_generation` invalidates stale futures on `refresh()`; `m_pendingLoads` (in-flight metadata futures) drives the `scanning` flag. `m_pathIndex` (`QHash<QString, int>`) keeps row index by path for O(1) `findRowByPath` |
| `AppImageSortFilterModel` | `appimagesortfiltermodel.h/.cpp` | Proxy over `AppImageListModel`. Sort: `SortByName=0`, `SortBySize=1`, `SortByCategory=5`, `SortByDate=9`. Filter: case-insensitive text match on `cleanName` + `appName`. `lessThan` reads typed values via `AppImageListModel::sortKey()` to skip QVariant role dispatch. Uses Qt 6.9 `beginFilterChange/endFilterChange` where applicable |
| `DashboardWindow` | `dashboardwindow.h/.cpp` | Singleton dashboard host. Creates its own `QQmlApplicationEngine`. Holds `m_listModel`, `m_proxyModel`, `m_uninstallBackend`, `m_storeModel`. `createBackend(path, withCorpses)` — uninstall backend passes `true` (triggers corpse scan after metadata load). Exposes `amStoreModel` as context property |
| `AMStoreModel` | `amstoremodel.h/.cpp` | Discover page store catalog. Two data sources: AM database (`programs/x86_64-appimages`, `◆ package : description` lines) and AppImage Hub feed (`appimage.github.io/feed.json`, 1,383 curated apps with icons, categories, authors). Both fetched in parallel; **parsing runs on worker threads** via `QtConcurrent::run` + `QFutureWatcher<QVector<StoreApp>>`; results marshalled back via `onAmParsed`/`onFeedParsed`. Disk cache in `QStandardPaths::CacheLocation` with 24h freshness check. `checkAllLoaded()` merges sources on a worker thread; `QIcon::hasThemeIcon()` is called **inside the worker** (Qt 6: `QIconLoader` is mutex-guarded, safe off main thread) to clear non-URL, non-theme iconSources before the result is moved to the main thread. `applyFilter()` dispatched to worker with `m_filterGeneration` counter to discard stale results; without a category filter it sorts by first category (empty last) then name, so the list can show category sections (`sectionCategory` role). `storeSource` (0 = all, otherwise the 1-based position in `sources`) persisted in `QSettings`; `setSourcesOrder()`/`removeSource()` move it with its catalog. SVG icons filtered at parse time — Qt's renderer crashes on CSS mesh-gradient SVGs. **Lazy-loaded: never call `load()` from constructor**; call `initialize()` once from QML when Discover page first becomes visible (and `AppSettings.discoverEnabled` is on). Source mutators (`addSource`/`updateSource`/`removeSource`/`resetSourcesToDefault`) only `sync()` once initialized, so toggling sources in onboarding/settings causes no network traffic before Discover is opened |

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
| `ManageWindow.qml` | `ApplicationWindow` (fixed 26×22 gridUnits) | Context prop: `backend` (AppImageBackend). Drag-and-drop install — floating `dragIcon` surrogate used as drag target, original icon fades to 30% opacity during drag. No box or chips: name, one dim `version · size` line, icons at `iconSizes.huge`. On drop calls `backend.beginInstall()` (not `installAppImage`); `onSignatureCheckReady` handler routes to `doInstall()` or opens `signatureWarnDialog`. Inline `UninstallDialog` and `signatureWarnDialog` (`Kirigami.Dialog` with security icon, Ok/Cancel) |
| `DashboardWindow.qml` | `Kirigami.ApplicationWindow` | Context props: `listModel`, `proxyModel`, `dashboardController`, `amStoreModel`. A `Controls.ToolBar` on top of a `StackLayout` of 4 pages: Installed (`LibraryPage`, 0, default), Discover (`StorePage`, 1), Settings (2), About (3). Toolbar: checkable Installed ("Installed (N)" while `listModel.updateCount > 0`) / Discover tabs, Update All (Installed tab, `updateCount > 1`), `+` Install from File, main menu (Check for Updates, Settings, About). Settings/About act as pushed pages: the tabs give way to Back + title; `goBack()` returns to `lastTab`; Alt+Left goes back, Ctrl+, opens Settings. Global drag-and-drop overlay for `.AppImage` files |
| `UpdateSourceDialog.qml` | `Kirigami.Dialog` | Per-AppImage update source: one field, live "detected as" line from `listModel.describeUpdateSourceInput()`, a Type menu to force the kind, pre-releases only for repositories. Opened via `openFor(proxyRow, name)` from the LibraryPage detail `⋯` menu. Cancel is a standard button; Save and Use Built-in are `customFooterActions`. Repo field (prefilled with the override or the embedded GitHub repo), "Include pre-releases", "Use Built-in" (clears the override). Saving the unchanged embedded repo without pre-releases stores no override. Signal `saved(cleared)` |
| `EnvironmentDialog.qml` | `Kirigami.Dialog` | Per-AppImage environment: `openFor(proxyRow, name)` from the LibraryPage `⋯` menu; monospace `TextArea`, one `KEY=value` per line; `proxyModel.setEnvironment()` returns invalid line numbers, shown in an `InlineMessage`. Signal `saved()` |
| `LibraryPage.qml` | `Kirigami.Page` | Context props: `listModel`, `proxyModel`. Master-detail layout. Left pane: search + Sort button (menu), thin progress bar (scanning or checking), error `InlineMessage` with Retry for failed update checks ("up to date" / "N updates" are passive notifications), list pane is a `Rectangle` with the View color set (panel); rows = `Kirigami.IconTitleSubtitle` (name, dim version) + update dot + busy spinner; `Kirigami.ListSectionHeader` sections by `sectionCategory`/`sectionAdded` when sorted by category/date; optional drop box below the list (outlined, fills the remaining height, minimum 6 gridUnits; the list is capped at its content height so it scrolls above the pinned box). Right pane (width capped at 38 gridUnits for readable lines): icon + level-1 name + dim developer · homepage, actions row (Launch, "Update to X" or a progress bar while updating, `⋯` menu: Show in App Menu, Update Source…, Environment Variables…, Remove…), "About" section (description) and "Details" key/value grid (Version, Size, Category, Added, Updates, Environment, File). `displayedItem` lags `currentItem` (80ms fade-out → swap → 150ms fade-in); data updates sync immediately. `updateAll()` starts every available update via the source model. Hosts `UpdateSourceDialog` and `EnvironmentDialog`. Shortcuts (only while visible): Return=Launch, Delete=Remove, Escape=clear search / deselect |
| `StorePage.qml` | `Kirigami.Page` | Discover page. Context prop: `amStoreModel`. Left pane: search + one Sort and Filter button (menu: Sort A–Z/Z–A, Source submenu, Category submenu); active filters as a dim line with a clear button; list pane is a View-colored panel; rows = `Kirigami.IconTitleSubtitle` (name, author), generic icon when none, grouped under `sectionCategory` headers unless a category is filtered. A model reset (filter change) clears the selection. Right pane: same layout as LibraryPage; one dim line of categories · license · sources. Primary action "Install" (AM) or "Download" (Hub); `⋯` offers the other source when both exist. While installing: progress bar (stage 0–5) + Cancel, a dim status line (last log line) and "Show Details" for the plain monospace log, which opens by itself on failure. Lazy init: `initializeIfNeeded()` (on visible change and on `discoverEnabledChanged`) calls `amStoreModel.initialize()` once, only when `AppSettings.discoverEnabled`. When off, a `PlaceholderMessage` with "Turn On Discover" replaces the page |
| `SettingsPage.qml` | `Kirigami.Page` | `AppSettings` singleton. FormCard-based form. Sections: General (folder, drop zone, Downloads notifications, install notifications), Updates, Security, Discover (Enable switch), Catalogs (only while Discover is on; reorder/edit/delete/add/reset). Restore Defaults is a `FormButtonDelegate` at the bottom |
| `AboutContent.qml` | `ColumnLayout` | The About content (app info, license, links, library versions, contributors via a `ContributorsModel` instance). Shared by `AboutPage` and `AboutDialog`; call `fetchContributors()` when shown |
| `AboutPage.qml` | `Kirigami.Page` | ScrollView around `AboutContent` (dashboard sub-page from the main menu) |
| `AboutDialog.qml` | `Kirigami.Dialog` | ScrollView around `AboutContent`, for `ManageWindow` |
| `UninstallDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Corpse checklist + total size + confirm. `Kirigami.InlineMessage` shows KIO trash errors. Sets `backend = null` on close |
| `OnboardingDialog.qml` | `Kirigami.Dialog` | Single page shown on `AppSettings.firstRun == true`: Applications folder, Downloads notifications, Enable Discover + per-catalog checkboxes from `amStoreModel.sources`, Dolphin tip, "Get Started". `onOpened` sets `discoverEnabled = false` on first run, making Discover opt-in for new installs; `onClosed` clears `firstRun`. `closePolicy: NoAutoClose`, no close button. `parentWindowWidth` property required — QML file scope does not inherit outer document IDs, so `root.width` is inaccessible; the width is passed explicitly from `DashboardWindow.qml`. `Layout.preferredWidth: 0` on all long labels prevents them from driving the dialog beyond `preferredWidth` (Qt layouts use `implicitWidth` to compute parent `implicitWidth`; wrapping text still reports its unwrapped natural width) |
| `ContributorsModel.qml` | QtObject (instantiated, not a singleton) | Contributor data and fetch logic (XHR to GitHub API, bot-filtered, fallback list). Instantiated once inside `AboutContent.qml` |

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
       └─ per item: updateInfo = effectiveUpdateInfo(embedded, loadUpdateSource(path))   [user's source wins]
            ├─ gh-releases-zsync / forgejo-releases → ReleaseChecker (Releases API; list when pre-releases allowed)
            │                      → pickAsset(): .zsync, else arch-matching .AppImage
            ├─ direct|url        → ReleaseChecker HEAD: Last-Modified/ETag vs installedTag (or file mtime)
            └─ plain zsync| URL  → fetch .zsync header, parse SHA-1, hash local file off-thread
       └─ updateFound(path, version, url) → role updates → update dot, "Update to X", "Installed (N)"
  └─ user clicks Update to X → proxyModel.downloadUpdate(row)   (Update All loops over listModel rows)
       └─ AppImageUpdateManager::downloadUpdate(...)
            ├─ .AppImage URL          → direct HTTP download to old.new (release without .zsync, direct source); web page → refused
            ├─ zsync2 on PATH         → QProcess zsync2 -i old -o old.new URL
            └─ zsync2 missing         → fetch .zsync header for URL:, full HTTP download to old.new
            └─ on success: rename old→old.bak, rename old.new→old, remove bak, chmod +x, downloadFinished(path, true)
                           (on rename failure: restore bak, remove new, emit failure)
            └─ release-type source: saveInstalledTag(path, version) so version-less AppImages stop re-offering it
```

### Discover page: store catalog load

```
StorePage.qml initializeIfNeeded() (first open with AppSettings.discoverEnabled)
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
- `AppImageManager::desktopFilePath(info)` → `~/.local/share/applications/<appId>.desktop` when `appId` is reverse-DNS and no system package has that id, else `appimage_<fileKey>.desktop` (the pre-2.1.1 name)
- `AppImageManager::iconFilePath(info)` → `~/.local/share/icons/hicolor/<size>/apps/appimage_<fileKey>.<ext>`
- `fileKey` = clean-name slug + hash of the file name (see `AppImageManager`).

### QML patterns
- All windows: `Kirigami.Theme.colorSet: Kirigami.Theme.Window` + `Kirigami.Theme.inherit: false`.
- Animated show/hide: `visible: opacity > 0` + `Behavior on opacity { NumberAnimation }`.
- Dialogs: take a `backend` (or domain-specific) property, reset it to `null`/defaults in `onClosed`.
- `UninstallDialog` / `UpdateSourceDialog` are dialogs, not windows — instantiated inline in their parent.
- Keep it quiet: stock Kirigami controls and theme colors only (no custom borders or palettes), dim secondary text via `Kirigami.Theme.disabledTextColor`, one metadata line instead of chips, a primary action plus a `⋯` menu, passive notifications or `InlineMessage`s instead of modal dialogs (modals only for destructive actions).
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
| **`AppImageCache` version** | Cache is versioned (`kCacheVersion = 5`, SQLite-backed; 5 re-reads every AppImage once after the appId fix). Adding new fields to `AppImageInfo` that must be persisted requires bumping `kCacheVersion`, adding a column to the `CREATE TABLE metadata` schema, and adding the field to both `load()` and `store()`. Forgetting any of the three silently returns empty strings for the new field |
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
| **`Kirigami.Page` actions outside a PageRow** | The dashboard pages live in a `StackLayout`, not `pageStack`, so a page's `actions`, `title` and global toolbar are never rendered. Sort and Restore Defaults were unreachable this way. Put controls in the page content or in `DashboardWindow`'s toolbar |
| **Lists go inside `Controls.ScrollView`** | A bare `ListView` with an attached `ScrollBar` lays the scrollbar over the rows (selection highlights run under it) and scrolls slowly with the mouse wheel. The desktop-style `Controls.ScrollView` pads the content for the scrollbar and installs `Kirigami.WheelHandler`. Put the sizing (`anchors`/`Layout.*`) on the ScrollView, not the ListView |
| **Delegate property named after a context property** | `property int index: index` in a delegate binds the property to itself (always 0), not to the model's `index`; the Settings catalog drag-reorder was dead because of it. Use another name (`readonly property int itemIndex: index`) |
| **Remote icons in long lists** | `Kirigami.Icon` with an http(s) source decodes on the UI thread; Discover rows use `Image { asynchronous: true; sourceSize: 2 × icon size }` for remote icons and `Kirigami.Icon` only for theme names |
| **Detail pane `displayedItem` pattern** | `LibraryPage` and `StorePage` both use a `displayedItem` property that lags behind `currentItem`. On selection change: `currentItem` updates immediately (so action-bar logic stays correct), then `SequentialAnimation` fades `detailInner` to 0 → ScriptAction updates `displayedItem` → fades back to 1. Data-only updates (metadata loaded, update check result) bypass the animation and sync `displayedItem` directly to avoid flickering on in-place updates |
| **`installAppImage()` removed from `AppImageBackend`** | The old single-step `installAppImage()` slot is gone. Use `beginInstall()` → wait for `signatureCheckReady(state)` → call `doInstall()`. QML in ManageWindow already does this via the `Connections` handler. Any test or external code that called `backend.installAppImage()` directly must be updated |
| **`appId` comes from the inner `.desktop` file name** | `AppImageReader` uses `QFileInfo::completeBaseName()` (`org.foo.Bar.desktop` → `org.foo.Bar`). `baseName()` stops at the first dot and turned ids into `org`/`io`/`ai`; `computeIconSource()` then preferred an unrelated theme icon of that name over the AppImage's own icon. Fixed with cache version 5 |
| **`fileKey()` hash suffix is a 2.0 breaking change** | Apps now get a `_<hash>` suffix in their `.desktop` and icon filenames. Pre-2.0 desktop links (without the suffix) will no longer be detected by `isDesktopLinkEnabled()`. Users must re-toggle those links once after upgrading. Do not add a migration fallback that checks the legacy path — it would mask the slug collision bug the change was made to fix |
| **Update sources and environment are keyed by file name** | `[UpdateSources][<file name>]` and `[Environment][<file name>]` in `appimagemanagerrc`, so a moved Applications folder keeps them (updates keep the path), but renaming an AppImage loses them. Both loaders call `reparseConfiguration()` because the dashboard, manage windows and daemon are separate processes. Saving an environment rewrites an existing app menu entry so its `Exec=` matches |
| **`discoverEnabled` defaults to `true`** | Deliberate: existing installs keep Discover without a migration. New installs get opt-in behaviour because `OnboardingDialog.onOpened` sets it to `false` on first run. "Restore Defaults" turns it back on |
| **No PKGBUILD in the repo root** | makepkg uses `<PKGBUILD dir>/src` as `$srcdir`; in the root that is the project's `src/` (and `makepkg -C` deletes it). The local PKGBUILD lives in `packaging/` and builds `$startdir/..` |
| **Apps write their own menu entries** | Some AppImages (and vendor launchers such as ArtCraft Launcher) write `~/.local/share/applications/<their id>.desktop` themselves. Never assume our file is the only entry: detect by `Exec` target, name ours after the app's id, and only ever delete our own `appimage_*` files automatically |
| **Dolphin plugin and servicemenu both ship** | `src/plugin/` (KFileItemAction `.so`) and `data/servicemenus/appimagemanager.desktop` both add "Manage AppImage" (same command). Undecided which to keep — ask before removing either |
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
8. Dashboard search → filters live. Sort button (next to search) → by name/size/category/date → list re-orders.
9. Dashboard detail `⋯` → Remove… → `UninstallDialog` → list refreshes after trash.
10. Main menu → Check for Updates → thin progress bar; none found → "All AppImages are up to date" notification; network off → error message above the list with Retry. Update found → update dot, "Installed (N)" tab, "Update to X" button → download starts directly, progress replaces the button; with 2+ updates "Update All" in the toolbar starts them all. Both `zsync2` present and absent (rename it temporarily) should succeed.
11. Settings → browse button opens folder picker → valid path saved → invalid path shows error.
12. Settings → disclaimer toggle persists across restart.
13. Select an AppImage with AppStream metadata (e.g. ProtonUp-Qt) → one dim metadata line, description flows inline below the actions; one scrollbar for the whole detail pane; no horizontal scrollbar.
14. Settings → "Notify when an AppImage is downloaded" on → copy a `.AppImage` into `~/Downloads` → KDE notification fires after settle pass → click "Manage" → ManageWindow opens for that file.
15. Toggle download notification off → copy another AppImage to `~/Downloads` → no notification fires.
16. Start daemon (`appimagemanager --daemon`) → tray entry appears with "Check Now" + "Open Dashboard". Open dashboard while daemon runs → drop a file in `~/Downloads` → only one notification fires (daemon wins).
17. AboutDialog: open dialog → disable network (`nmcli networking off`) → reopen About → wait 8 s → spinner hides, fallback contributors shown, no SIGABRT. Re-enable network, reopen → real contributors load.
18. Sandbox: run `FLATPAK_ID=test appimagemanager`, open Settings → "Download watching is unavailable in sandboxed environments." InlineMessage visible below toggle. Copy a `.AppImage` to `~/Downloads` → no notification fires.
19. Update: trigger update on a test AppImage → verify `.bak` file created mid-swap; on success `.bak` removed. Kill process during swap → `.bak` present afterward for manual recovery.
20. Launch app → Installed tab shown immediately; Discover NOT yet loaded (no network traffic at startup).
21. Click Discover → spinner visible immediately → list populates after a few seconds with no UI freeze. Close and reopen Discover → loads instantly from cache.
22. Discover search → type "firefox" → results filter live. Sort and Filter button → Sort A–Z / Z–A; Source → "AppImage Hub" → only Hub apps; Category → pick one. Active filters show as a dim line under the search; ✕ clears them. Changing a filter clears the selection.
23. Discover: app with both AM + Hub → "Install" plus `⋯` offering "Download from AppImage Hub". App with only AM → "Install", no `⋯`. App with only a Hub download → "Download".
24. Header: Settings and About open from the main menu with Back + title in place of the tabs; Back and Alt+Left return to the last tab; Ctrl+, opens Settings. Settings → Restore Defaults (bottom) resets values and the Enable Discover switch.
25. Discover: click Install → progress bar + Cancel replace the button, one dim status line; "Show Details" shows the plain log. A failed install opens the log by itself. Cancel mid-install → "Installation cancelled." in the log, button resets.
26. Settings → enable "Verify GPG signatures". Drag an unsigned AppImage to install → `signatureWarnDialog` appears with "No Signature Found" title and security-medium icon. Click Cancel → no install. Click OK → install proceeds normally.
27. With signature verification on, drag an AppImage that has a valid GPG signature → no dialog, installs directly.
28. With signature verification OFF → drag any AppImage → installs immediately, no dialog regardless of signature state.
29. Install two AppImages with the same display name but different filenames, neither having a reverse-domain `appId` → both get unique `.desktop` files in `~/.local/share/applications/` (slugs differ by hash suffix); `isDesktopLinkEnabled` correctly tracks each independently.
30. Update source: pick an AppImage with no built-in update info → detail `⋯` → Update Source… → paste a GitHub `owner/repo`, a Codeberg/Forgejo repository URL, a `.zsync` URL or a direct `.AppImage` URL → the line under the field names the detected kind (Type menu overrides) → Save → Details shows it under Updates → "Check for Updates" → a newer release (or a newer build for a direct URL) is offered; Update downloads it. A second check reports it up to date. "foo" shows the error. "Use Built-in" clears it.
31. Update source with "Include pre-releases" on a repo whose newest release is a pre-release → that version is offered; off → only the latest stable.
32. Fresh config (`mv ~/.config/appimagemanagerrc{,.bak}`) → single-page onboarding with Discover off → Discover page shows "Discover is turned off" and no network requests happen; "Turn On Discover" loads the catalogs. Get Started without enabling → Discover stays off. Restore the config afterwards.
33. `cd packaging && makepkg -si` → package builds and installs; `pacman -Ql appimage-manager-plasma` lists the binary, plugin, and QML module.
34. Environment: detail `⋯` → Environment Variables… → `WEBKIT_DISABLE_DMABUF_RENDERER=1` → Save → Details lists it; Launch and the app menu entry both start the app with it (`cat /proc/<pid>/environ`). A line like `not a var` is refused with its line number.
35. Direct download URL that answers with a web page (e.g. a project's download landing page) → the check fails quietly / the download is refused and the installed AppImage stays intact.
36. Sort Installed by category and by date added → section headers appear; Discover without a category filter shows category headers, with one filtered none.
37. Settings → Catalogs: drag a catalog by its handle → it moves and the new order sticks after reopening Settings. With Discover filtered to a catalog, reorder or remove another one → the filter stays on the same catalog. Edit a turned-off catalog → it stays off.
38. Drag an AppImage over the window on Discover, Settings, and an empty Installed list → the whole window highlights with "Drop to install"; on Installed with the drop box shown only the box highlights.
39. An app that writes its own menu entry (e.g. ArtCraft's `ai.storyteller.*`) next to an old `appimage_*` entry → open the dashboard → only the app's entry remains, "Show in App Menu" is on; turning it off removes it (Trash), on writes `<appId>.desktop` again.
