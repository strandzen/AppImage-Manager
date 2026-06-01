# AppImage Manager — Deep Improvement Plan

## Context

A senior-dev pass over the current `main` (v1.2.1 + uncommitted WIP for the new `AppImageUpdateManager` / `DownloadWatcher` split). The codebase is in good shape, but the recent refactor of `AppImageListModel`, the SQLite cache migration, and the new update/download split introduced real correctness, perf, and KDE-integration regressions that should be fixed before the next release.

Goals:

1. **Correctness** — eliminate latent threading & lifetime defects, silent failure paths, and duplicated logic between `UpdateDaemon` and `DownloadWatcher`.
2. **Perceived performance** — get cold-start under 500 ms for ~100 cached AppImages and remove icon-decode jank.
3. **KDE-native polish** — replace hand-rolled boilerplate with KDE Frameworks 6 equivalents (KConfigXT, KDirWatch, `.notifyrc`, `KDBusService::Unique` for the dashboard, AppStreamQt).
4. **Test coverage** — commit the existing test scaffold and extend it so future refactors don't regress silently.
5. **Tooling baseline** — bump Qt to 6.9 / KF6 to 6.7 to unlock newer APIs (`beginFilterChange`/`endFilterChange`, modern Kirigami).

Outcome: a release candidate (v1.3.0) that is measurably faster, has no latent crashes in normal use, and feels more Plasma-native.

Stale-doc note: [CLAUDE.md](CLAUDE.md) still claims the cache is `QSettings` INI — actually SQLite since v4 ([src/core/appimagecache.cpp](src/core/appimagecache.cpp)). Update it as the last sub-task of every phase.

---

## Phase 0 — Pre-flight (do once, blocks nothing)

- Commit the uncommitted [tests/](tests/) tree as a baseline so Phase 4 can land additions, not the whole suite, in one PR.
- Bump `CMakeLists.txt:24` to `Qt6 6.9 REQUIRED` and `KF6 6.7 REQUIRED`. Drop the `if (KF6StatusNotifierItem_FOUND)` guard — `StatusNotifierItem` is unconditional in KF6 ≥ 6.0; the guard is redundant.
- Update [CLAUDE.md](CLAUDE.md) cache description to SQLite + `cacheVersion = 4`.

---

## Phase 1 — Correctness fixes

### 1.1 `DownloadWatcher` no-op set ops (real defect)

**Files:** [src/gui/downloadwatcher.cpp:60-62](src/gui/downloadwatcher.cpp#L60-L62)

`m_known.intersect(current); m_known.unite(current);` is functionally `m_known = current;`. Comment claims "Keep known in sync so removed-then-re-added files fire again correctly" — but the current code does fire again because it only adds to `m_known` after notification. Replace with a single assignment.

### 1.2 Mid-download notification storm

**Files:** [src/gui/downloadwatcher.cpp](src/gui/downloadwatcher.cpp) + [src/core/updatedaemon.cpp:82-118](src/core/updatedaemon.cpp#L82-L118)

`QFileSystemWatcher::directoryChanged` fires on every write while a browser is still streaming the AppImage to disk. The current code notifies immediately on first appearance — the user gets "Krita downloaded" 30 seconds before the file is actually usable. Worse, if the user clicks "Manage", `AppImageReader::read()` fails on an incomplete file.

Fix: add a per-file settle timer in `DownloadWatcher`. When a new path appears, record `QFileInfo::size()` and `lastModified()`, then start a 1.5 s `QTimer`; on tick, re-stat — if size+mtime unchanged, emit `appImageFound`; otherwise restart the timer. Cap at ~5 retries to avoid permanent pending.

### 1.3 De-duplicate Downloads watching between daemon and dashboard

**Files:** [src/core/updatedaemon.cpp:66-118](src/core/updatedaemon.cpp#L66-L118), [src/gui/downloadwatcher.cpp](src/gui/downloadwatcher.cpp), [src/gui/appimagelistmodel.cpp:53-70](src/gui/appimagelistmodel.cpp#L53-L70)

Both the daemon and the dashboard own download-watching code (~80 lines duplicated). Coordination is via the `io.github.appimagemanager.Daemon` D-Bus name check. The post-1.3 refactor should:

- Move all download-watching into `DownloadWatcher` (single source of truth, used by both the daemon and the dashboard).
- Have the daemon instantiate one `DownloadWatcher` directly.
- Drop `UpdateDaemon::watchDownloads()` / `onDownloadDirChanged()` / `m_knownDownloads` / `m_downloadWatcher`.
- The dashboard keeps its `DownloadWatcher` but skips emission when the daemon D-Bus name is registered (unchanged behavior).

### 1.4 Silent failures in `appimagemanager.cpp`

**Files:** [src/core/appimagemanager.cpp:62-108](src/core/appimagemanager.cpp#L62-L108)

`createDesktopLink()` and `removeDesktopLink()` always `return true` regardless of `QFile::write()`/`QFile::remove()` outcome. The dashboard then thinks the toggle succeeded and updates UI state — but the link wasn't actually written/removed.

Fix: capture each filesystem op's bool, return `false` on first failure, log via `qCWarning(AIM_LOG)`. Callers in [src/gui/appimagelistmodel.cpp:332-343](src/gui/appimagelistmodel.cpp#L332-L343) and [src/gui/appimagebackend.cpp:116-125](src/gui/appimagebackend.cpp#L116-L125) already check the return — just need it to actually mean something.

### 1.5 `KIO::trash` ignored result in `trashSelected`

**Files:** [src/gui/appimagelistmodel.cpp:445](src/gui/appimagelistmodel.cpp#L445)

`KIO::trash(urls)->start();` discards the job. If trash fails (full trash, permission issue), the user sees nothing and the items remain on disk. Connect `KJob::result` and route failures to `sendError()` (already a static helper at [src/gui/appimagelistmodel.cpp:464](src/gui/appimagelistmodel.cpp#L464)).

### 1.6 Inconsistent sycoca rebuild

**Files:** [src/gui/appimagebackend.cpp:198](src/gui/appimagebackend.cpp#L198), [src/core/appimagemanager.cpp:93](src/core/appimagemanager.cpp#L93)

Only the install path calls `kbuildsycoca6`. Toggling the desktop link from the dashboard writes `.desktop` files but never rebuilds the cache, so Plasma's launcher doesn't pick up the change until the next manual rebuild. Move the `QProcess::startDetached(QStringLiteral("kbuildsycoca6"), {})` call into a shared helper (`AppImageManager::rebuildSycoca()` — already referenced in [CLAUDE.md](CLAUDE.md) but not present in the current code) and invoke from both `createDesktopLink()` and `removeDesktopLink()`.

### 1.7 `AppSettings::sync()` per setter

**Files:** [src/core/appsettings.cpp](src/core/appsettings.cpp) (every setter)

KConfig syncs on app exit and via internal timer. Calling `m_config->sync()` after every property write forces a `fdatasync()` on the config file each time. With the settings dialog, a single tab switch can trigger 5+ fdatasyncs. Drop `sync()` from every setter; the destructor already syncs.

### 1.8 `UpdateDaemon::checkUpdates` blocks worker pool on serial reads

**Files:** [src/core/updatedaemon.cpp:192-198](src/core/updatedaemon.cpp#L192-L198)

`QtConcurrent::run` runs a lambda that loops over `files` and calls `AppImageReader(...).read()` **serially in a single worker thread**. Because the reader hits the cache (cheap) for already-known files, this is fast in steady state — but on first daemon run after a cache wipe, scanning 100 AppImages takes minutes single-threaded.

Fix: use `QtConcurrent::mapped` over `files` so the reads parallelize across the global thread pool. Result is still a `QList<AppImageInfo>`.

### 1.9 `m_lastTrashedUrls` accumulation across calls

**Files:** [src/gui/appimagebackend.cpp:168-178](src/gui/appimagebackend.cpp#L168-L178)

`m_lastTrashedUrls.clear()` runs at the *start* of `removeAppImageAndCorpses`, so if a second call lands before the first job finishes (UI shouldn't allow this but defensive code), the in-flight job's `copyingDone` lambda mutates the URL list belonging to the *new* call. Move `clear()` into `onRemoveJobFinished` after consuming, or scope the URL list to the job's lifetime via a `QPointer` capture.

---

## Phase 2 — Performance wins (user-visible)

### 2.1 Cache pre-check on main thread (cold-start win)

**Files:** [src/gui/appimagelistmodel.cpp:284-328](src/gui/appimagelistmodel.cpp#L284-L328), [src/core/appimagereader.cpp:36-39](src/core/appimagereader.cpp#L36-L39)

Today, every row spawns a `QtConcurrent::run` even when the cache is hot. The cache lookup is in `AppImageReader::read()` line 37 — i.e., inside the worker. For 100 cached AppImages that's 100 thread spawns just to do 100 SQLite point reads.

Fix:

1. Expose `AppImageCache::loadFast(path, mtime)` (already effectively `load()`; just document it as main-thread-safe).
2. In `loadMetadataForRow`, call `AppImageCache::instance().load(path, mtime)` **synchronously on the main thread**. If `isValid`, populate the item directly and skip the future. Decrement `m_pendingLoads` accordingly.
3. Only spawn `QtConcurrent::run` on a cache miss.

Expected: 100 cached AppImages → no thread spawns, ~50ms total. Current: ~3 s of `BusyIndicator` flicker.

### 2.2 Icon pixmap cache in `AppImageIconProvider`

**Files:** [src/gui/appimageiconprovider.cpp:25-75](src/gui/appimageiconprovider.cpp#L25-L75)

`requestPixmap` decodes raw bytes (PNG or SVG-via-`QSvgRenderer`) on every QML call. Same delegate scrolling in and out triggers re-decode. Add a `QCache<QString, QPixmap>` keyed by `id + ":" + size.width() + "x" + size.height()`, with a 32 MB cap.

Also: `setIconData()` should `m_cache.remove(prefixMatch)` to invalidate stale renders when an AppImage's icon changes.

### 2.3 Sort model: cache compare keys

**Files:** [src/gui/appimagesortfiltermodel.cpp:53-82](src/gui/appimagesortfiltermodel.cpp#L53-L82)

`lessThan` calls `.data()` 2× per compare per role. Each `.data()` returns a `QVariant` and goes through role-switch in `AppImageListModel::data()`. For 200 items: ~3 200 × 2 = 6 400 QVariant boxing operations per sort.

Fix: subclass `QSortFilterProxyModel::sort()` (or override `lessThan` to dispatch directly against `AppImageListModel::Item` via a friend access) and read into a pre-allocated `QVector<SortKey>` once, then sort indices. Alternatively expose `AppImageListModel::sortKey(row, field)` returning a `QVariant` populated from the `Item` struct directly (no role dispatch).

### 2.4 Disable transitions for bulk inserts

**Files:** [qml/DashboardWindow.qml:317-325](qml/DashboardWindow.qml#L317-L325)

`add`/`displaced` transitions animate every row for `longDuration` (~600 ms). Initial scan inserts N rows → N animations stack → ~10 s of "moving" UI for 100 items.

Fix: bind `add`/`displaced` transitions to `enabled: !listModel.scanning`. Quietly fill the list on initial scan; re-enable animations once `scanning === false` so single user-driven adds still animate.

### 2.5 Detail-pane fetches all 24 roles on selection change

**Files:** [qml/DashboardWindow.qml:31-53](qml/DashboardWindow.qml#L31-L53)

`refreshCurrentItemAt` calls `proxyModel.data()` 18 times per selection change. Each call traverses the proxy → source mapping → role switch.

Fix: add `Q_INVOKABLE QVariantMap AppImageListModel::itemData(int row) const` returning all roles in one call. Update QML to `currentItem = proxyModel.itemData(currentIndex)`.

### 2.6 `dataChanged` over-fetches roles

**Files:** [qml/DashboardWindow.qml:182-188](qml/DashboardWindow.qml#L182-L188)

When `loadMetadataForRow` finalizes, it emits `dataChanged` for the row, which calls `refreshCurrentItemAt` and re-fetches everything. Use the `roles` argument from `onDataChanged(topLeft, bottomRight, roles)` to update only the changed fields in `currentItem`.

---

## Phase 3 — KDE-native modernization

### 3.1 `.kcfg` + `kcfgc` for `AppSettings`

**Files:** new `data/appimagemanager.kcfg`, new `data/appimagemanager.kcfgc`, drop most of [src/core/appsettings.cpp](src/core/appsettings.cpp)

Today: ~180 lines of hand-rolled getter/setter/signal boilerplate for 8 settings. With `kconfig_add_kcfg_files(...)`, a single XML schema generates the typed class + Q_PROPERTYs + change signals + default values. Keep `AppSettings::openFolderPicker()` and `applicationsPathError()` as a thin C++ subclass — the rest is generated. Net diff: −150 lines, type-safe, KCM-discoverable.

Why: less code to maintain, automatic schema-based defaults, plays nicely with KConfigWatcher for live cross-process updates.

### 3.2 `KDirWatch` over `QFileSystemWatcher`

**Files:** [src/gui/appimagelistmodel.cpp:37](src/gui/appimagelistmodel.cpp#L37), [src/gui/downloadwatcher.cpp:15](src/gui/downloadwatcher.cpp#L15)

`QFileSystemWatcher` has well-known limitations: doesn't recover from `mv`/`rm -rf`, fires once per parent dir change but doesn't tell you what changed. `KDirWatch` (`KF6::CoreAddons`) handles atomic renames and emits `dirty(path)` with the *changed file*, eliminating the full-rescan-on-any-change pattern.

For `AppImageListModel`: switch to `KDirWatch::self()->addDir(applicationsPath)`, listen to `created(path)`/`deleted(path)`/`dirty(path)`, and diff incrementally instead of full `refresh()`. Removes the 500 ms debounce timer (KDirWatch already coalesces).

### 3.3 `KDBusService::Unique` for the dashboard

**Files:** [src/main.cpp:43](src/main.cpp#L43), [src/gui/dashboardwindow.cpp:36-47](src/gui/dashboardwindow.cpp#L36-L47)

Today: `KDBusService::Multiple` so Dolphin can open N manage windows in N processes. But `appimagemanager --dashboard` from two different terminals creates two dashboards (the `s_instance` singleton is in-process only).

Fix: bifurcate. When invoked with a positional file argument or `--daemon`, use `KDBusService::Multiple`. When invoked dashboard-only (no args or `--dashboard`), use `KDBusService::Unique` with `appimagemanager.dashboard` service name. Handle `KDBusService::activateRequested` to raise the existing dashboard.

### 3.4 `.notifyrc` for KNotification events

**Files:** new `data/appimagemanager.notifyrc`

Today: events fire with hard-coded event IDs (`"downloaded"`, `"installed"`, `"updateAvailable"`, `"updateDownloaded"`, `"removed"`, `"error"`, `"installFailed"`). Without a `.notifyrc`, users cannot customize sounds/popups per event in the System Settings → Notifications KCM, and the events appear under a generic "appimagemanager" entry with no human-readable names.

Add `data/appimagemanager.notifyrc` declaring each event with localized name/comment/icon/default action. Install under `${KDE_INSTALL_KNOTIFYRCDIR}`.

### 3.5 Persistent tray status (always-on, not only on updates)

**Files:** [src/core/updatedaemon.cpp:201-224](src/core/updatedaemon.cpp#L201-L224)

`KStatusNotifierItem` is currently created lazily only when an update is found. A user with no pending updates has no way to know the daemon is running, or to manually trigger a check.

Make the tray item created at daemon start, status `Passive` when idle, `Active` when updates pending. Provide a `Check Now` action and an `Open Dashboard` default action.

### 3.6 `AppStreamQt` for richer metadata (optional, post-1.3)

**Files:** [src/core/appimagereader.cpp:311-366](src/core/appimagereader.cpp#L311-L366)

Today: hand-rolled `QXmlStreamReader` extraction with manual `<br>` injection and HTML escaping. Brittle (e.g., `<em>` / `<code>` aren't handled), and we miss screenshots, project URL, license, and rating.

If `libAppStreamQt` (`appstream-qt6`) is found at configure time, parse the extracted XML via `AppStream::MetadataPool` → get a `AppStream::Component` with proper field access. Falls back to current `extractMetadataFromXml` if AppStreamQt is unavailable. Lets the dashboard show screenshots and project links in a future iteration.

Mark this **Phase 3.6 — defer to v1.4** if scope tightens.

### 3.7 Replace install-via-`KIO::move` ambiguity

**Files:** [src/core/appimagemanager.cpp:40-60](src/core/appimagemanager.cpp#L40-L60)

`installAppImage` is `KIO::move(source, dest)`. A user dragging an AppImage from `~/Downloads` to the dashboard expects "install", not "move my downloaded file away." If the AppImage was on a USB stick, the file vanishes from the stick.

Decision rule: if the source path is under `~/Downloads` → `KIO::move`; otherwise `KIO::copy`. Or always copy and offer "Remove original?" in a follow-up `KNotification` with a "Yes" action. Pick one; document in [CLAUDE.md](CLAUDE.md).

---

## Phase 4 — Test suite (commit + expand)

### 4.1 Commit existing

**Files:** [tests/CMakeLists.txt](tests/CMakeLists.txt), [tests/tst_appimagecache.cpp](tests/tst_appimagecache.cpp), [tests/tst_appimageinfo.cpp](tests/tst_appimageinfo.cpp), [tests/tst_appimagereader.cpp](tests/tst_appimagereader.cpp)

Already exist locally; just `git add tests/` and commit as part of Phase 0.

### 4.2 Add `tst_appimagesortfiltermodel`

Covers: filter case-insensitive matching, four sort fields, equal-key tie-break by display name.

### 4.3 Add `tst_appimagelistmodel`

Use `QSignalSpy` for `scanningChanged`, `dataChanged`, `rowsInserted`. Drive scans against a temp dir of fake `*.AppImage` files (zero-byte stubs OK — reader will fail gracefully and items still appear with `metadataLoaded=false`). Verify:

- Scanning state goes true → false correctly.
- Adding a file fires `rowsInserted` exactly once.
- Removing a file fires `rowsRemoved` exactly once.
- Generation bump invalidates stale futures (use a manual `QFutureSynchronizer` to assert lambda paths).

### 4.4 Add `tst_downloadwatcher`

Stub `QStandardPaths::DownloadLocation` to a temp dir via `QStandardPaths::setTestModeEnabled(true)`. Drop a file in, advance event loop with the settle timer (1.5 s in test), assert `appImageFound` fires once. Drop a same-named file again after delete — should fire again.

### 4.5 Add `tst_githubreleasechecker`

Inject a `QNetworkAccessManager` whose `get()` is intercepted via a mock that returns canned JSON. Cover: newer remote → `updateAvailable`, equal → `upToDate`, malformed JSON → `failed`, network error → `networkFailed`.

### 4.6 CI

Add `data/ci/test.sh` running `ctest --output-on-failure --test-dir build/dev`. Mention in [README.md](README.md).

---

## Phase 5 — Documentation + release

- Bump version in [CMakeLists.txt:2](CMakeLists.txt#L2) to `1.3.0`.
- Rewrite stale sections in [CLAUDE.md](CLAUDE.md): cache is SQLite (v4), `findCorpses` no longer uses a blacklist (matches by appId + name variants — verify against [src/core/appimagemanager.cpp:115-164](src/core/appimagemanager.cpp#L115-L164)), `AppSettings` is now generated from `.kcfg`, `kAppImageFilters()` already documented but mention `kCacheVersion = 4`.
- CHANGELOG: group by Phase 1/2/3 to make the release notes obvious.

---

## Critical files to modify (quick index)

| Phase | Files |
|------|-------|
| 1.1 | [src/gui/downloadwatcher.cpp](src/gui/downloadwatcher.cpp) |
| 1.2 | [src/gui/downloadwatcher.cpp](src/gui/downloadwatcher.cpp), [src/gui/downloadwatcher.h](src/gui/downloadwatcher.h) |
| 1.3 | [src/core/updatedaemon.cpp](src/core/updatedaemon.cpp), [src/core/updatedaemon.h](src/core/updatedaemon.h), [src/gui/appimagelistmodel.cpp](src/gui/appimagelistmodel.cpp) |
| 1.4 | [src/core/appimagemanager.cpp](src/core/appimagemanager.cpp) |
| 1.5 | [src/gui/appimagelistmodel.cpp](src/gui/appimagelistmodel.cpp) |
| 1.6 | [src/core/appimagemanager.cpp](src/core/appimagemanager.cpp), [src/core/appimagemanager.h](src/core/appimagemanager.h), [src/gui/appimagebackend.cpp](src/gui/appimagebackend.cpp) |
| 1.7 | [src/core/appsettings.cpp](src/core/appsettings.cpp) |
| 1.8 | [src/core/updatedaemon.cpp](src/core/updatedaemon.cpp) |
| 1.9 | [src/gui/appimagebackend.cpp](src/gui/appimagebackend.cpp), [src/gui/appimagebackend.h](src/gui/appimagebackend.h) |
| 2.1 | [src/gui/appimagelistmodel.cpp](src/gui/appimagelistmodel.cpp), [src/core/appimagecache.h](src/core/appimagecache.h) |
| 2.2 | [src/gui/appimageiconprovider.cpp](src/gui/appimageiconprovider.cpp), [src/gui/appimageiconprovider.h](src/gui/appimageiconprovider.h) |
| 2.3 | [src/gui/appimagesortfiltermodel.cpp](src/gui/appimagesortfiltermodel.cpp), [src/gui/appimagelistmodel.h](src/gui/appimagelistmodel.h) |
| 2.4 | [qml/DashboardWindow.qml](qml/DashboardWindow.qml) |
| 2.5 | [qml/DashboardWindow.qml](qml/DashboardWindow.qml), [src/gui/appimagelistmodel.cpp](src/gui/appimagelistmodel.cpp), [src/gui/appimagelistmodel.h](src/gui/appimagelistmodel.h) |
| 2.6 | [qml/DashboardWindow.qml](qml/DashboardWindow.qml) |
| 3.1 | new `data/appimagemanager.kcfg`, new `data/appimagemanager.kcfgc`, [src/core/appsettings.cpp](src/core/appsettings.cpp), [src/core/appsettings.h](src/core/appsettings.h), [src/CMakeLists.txt](src/CMakeLists.txt) |
| 3.2 | [src/gui/appimagelistmodel.cpp](src/gui/appimagelistmodel.cpp), [src/gui/appimagelistmodel.h](src/gui/appimagelistmodel.h), [src/gui/downloadwatcher.cpp](src/gui/downloadwatcher.cpp), [src/gui/downloadwatcher.h](src/gui/downloadwatcher.h) |
| 3.3 | [src/main.cpp](src/main.cpp), [src/gui/dashboardwindow.cpp](src/gui/dashboardwindow.cpp) |
| 3.4 | new `data/appimagemanager.notifyrc`, [data/CMakeLists.txt](data/CMakeLists.txt) |
| 3.5 | [src/core/updatedaemon.cpp](src/core/updatedaemon.cpp), [src/core/updatedaemon.h](src/core/updatedaemon.h) |
| 3.6 | [src/core/appimagereader.cpp](src/core/appimagereader.cpp), [CMakeLists.txt](CMakeLists.txt) |
| 3.7 | [src/core/appimagemanager.cpp](src/core/appimagemanager.cpp) |

---

## Reuse opportunities (already in the tree — do not re-implement)

- `KFormat::formatByteSize` — already used in [src/gui/appimagelistmodel.cpp:473-477](src/gui/appimagelistmodel.cpp#L473-L477); use the same helper in `CorpseModel` instead of re-rolling.
- `KQuickIconProvider` — already registered under `"icon"` scheme in both [dashboardwindow.cpp:76](src/gui/dashboardwindow.cpp#L76) and [appimagewindow.cpp:53](src/gui/appimagewindow.cpp#L53). Theme-icon icons should resolve here, never through `AppImageIconProvider`.
- `kAppImageFilters()` in [src/core/appimageinfo.h:56-60](src/core/appimageinfo.h#L56-L60) — use everywhere instead of inline `{"*.AppImage","*.appimage"}` literals.
- `normalizeVersion()` / `isNewerVersion()` in [src/core/appimageinfo.h:38-52](src/core/appimageinfo.h#L38-L52) — single source of truth for version semantics.
- `AppImageManager::removeItems()` returns a `KJob*` — connect to `KJob::result` everywhere instead of fire-and-forget.

---

## Verification

After each phase, manually run the existing [CLAUDE.md](CLAUDE.md) testing checklist (steps 1–15). In addition:

**Phase 1:**

- Drop a `.crdownload` rename event into `~/Downloads` (simulate browser download): verify only one notification fires, after settle delay, and that clicking "Manage" opens a fully-parsed window.
- Kill the daemon (`pkill -f 'appimagemanager --daemon'`), open the dashboard, drop an AppImage into Downloads: notification still fires from the dashboard.
- Toggle desktop link from the dashboard: verify entry appears/disappears in the launcher within ~1 s (sycoca rebuilt).
- Try removing an AppImage with `~/.local/share/Trash` full (`fallocate -l 100G ~/.local/share/Trash/files/filler`): verify error notification, not silent failure.

**Phase 2:**

- Drop 100 AppImages into a fresh `~/Applications`, wipe `~/.local/share/AppImageManager/cache.db`, launch dashboard, time first paint (should be ~3 s the first time, ~500 ms the second).
- `QML_DISABLE_DISK_CACHE=1 appimagemanager --dashboard` with profiler (`PERF_EVENTS=cycles` or `qtcreator → Analyze`), verify no `QSvgRenderer::render` on hot scroll paths.
- Sort 200 AppImages by Size → by Name → by Date: should be <100 ms each.

**Phase 3:**

- `appimagemanager --dashboard` from two terminals → second one should raise the first, not open a new window.
- `systemsettings5 → Notifications → Application Settings → AppImage Manager`: all events appear with localized names from `.notifyrc`.
- Toggle "Watch downloads" off in settings: `KDirWatch` removes its watch (verify via `lsof -p $PID | grep inotify`).
- Daemon tray icon visible always; right-click → "Check for updates" works.

**Phase 4:**

- `cmake --build --preset dev && ctest --test-dir build/dev --output-on-failure` — all tests pass.

**Phase 5:**

- `cmake --preset release && sudo cmake --install build/release` from clean checkout → working dashboard.
- Bump `data/io.github.strandzen.AppImageManager.metainfo.xml` (if it exists) with the new version + release notes; verify with `appstreamcli validate ...`.

---

## Out of scope (intentionally — do not let this plan grow)

- Signature verification (roadmap item per [CLAUDE.md](CLAUDE.md)). Deserves its own design pass.
- Replacing `KIO::CopyJob` with a custom `KJob` subclass for install. Current code is fine; no real benefit.
- KRunner plugin. Big new feature; separate proposal.
- Flatpak / KDE Store packaging. Distribution concern, separate from code health.
- Rewriting the QML `Flow` chip layout. The chip-visibility-toggle "issue" is theoretical; profile first.
