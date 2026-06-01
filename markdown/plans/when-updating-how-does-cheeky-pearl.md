# Plan: Codebase Improvements + Comment Pass

## Context

Full survey of AppImageManager codebase. Two deliverables:
1. **Improvements** — concrete bugs and quality issues worth fixing, presented with rationale
2. **Comments** — targeted additions where the WHY is genuinely non-obvious (per CLAUDE.md philosophy: no comments where code speaks for itself)

---

## Part 1 — Possible Improvements

### Bug-Level (correctness issues)

| # | Issue | File | Why it matters |
|---|-------|------|----------------|
| B1 | `isNewerVersion("1.10", "1.9")` returns false — dot-split segments compared as ints, but string-split + `toInt()` on "10" works correctly. WAIT: actually `toInt()` IS used, so "1.10" → 10 vs "1.9" → 9 → 10 > 9 → correct. No bug here. | `appimageinfo.h:47` | N/A |
| B2 | `m_updateCheckTimer` uses `Qt::SingleShotConnection` but if `checkForUpdates()` is called twice within 30 s, the first timer is still running — `start()` restarts it correctly, but the old `SingleShotConnection` lambda is still connected and will fire with stale `m_pendingUpdateChecks = 0`. Double-emits `updateCheckFinished`. | `appimagelistmodel.cpp:447` | Spurious "No updates found" dialog if user clicks Check Updates twice fast |
| B3 | In `downloadUpdate()`, file ops are chained without early exit: `QFile::remove(oldFile)` then `QFile::rename(newFile, oldFile)`. If `remove()` fails, `rename()` still runs — leaves `.new` file at `oldFile` path. | `appimagelistmodel.cpp:589-595` | Corrupts the AppImage if disk is full or file is locked |
| B4 | `UpdateDaemon::checkUpdates()` reimplements the GitHub release API check inline (lines 129–196) instead of using `GitHubReleaseChecker`. Logic diverges — daemon uses different JSON parsing, different error handling, no `networkFailed` distinction. | `updatedaemon.cpp:129-196` | Bugs fixed in one place won't apply to the other; dual maintenance burden |

### Performance

| # | Issue | File | Why it matters |
|---|-------|------|----------------|
| P1 | Regex `R"(\s+\d[\d.]*$)"` in `downloadUpdate()` lambda compiled on every `readyReadStandardOutput` signal, potentially hundreds of times per update. | `appimagelistmodel.cpp:571` | Static regex costs nothing; repeated compilation wastes CPU |
| P2 | `AppImageCache::store()` calls `settings.sync()` on every write. Dashboard scans 20 apps → 20 sync() calls. | `appimagecache.cpp:93` | `sync()` flushes to disk synchronously; batch or defer to destructor |

### Robustness

| # | Issue | File | Why it matters |
|---|-------|------|----------------|
| R1 | `QDir().mkpath(dir)` return value ignored in `AppImageCache`, `AppSettings`, `AppImageManager` (3+ places). Silent failure means subsequent writes go to wrong/missing dir. | `appimagecache.cpp:19`, `appsettings.cpp:37`, `appimagemanager.cpp:42` | Path errors fail silently; log the failure at minimum |
| R2 | `GitHubReleaseChecker` sets no per-request timeout on `QNetworkReply`. Qt default is no timeout. The 30 s global timer in `AppImageListModel` covers the batch, but one stalled request blocks all others from completing until the batch timer fires. | `githubreleasechecker.cpp:35` | A single stalled GitHub request stalls all others for 30 s |
| R3 | `AppImageCache` has no eviction: INI file grows by one entry per unique AppImage ever opened. After hundreds of installs/removals the file becomes large and slow. | `appimagecache.cpp` | Long-term: stale entries accumulate forever |

### Duplication / Architecture

| # | Issue | File | Why it matters |
|---|-------|------|----------------|
| A1 | (see B4) `UpdateDaemon` duplicates GitHub check logic instead of reusing `GitHubReleaseChecker`. | `updatedaemon.cpp` | Same as B4 |
| A2 | Home-path substitution (`path.startsWith(QDir::homePath()) ? "~" + path.mid(...)`) duplicated in `UninstallDialog.qml` and `StorageDialog.qml` with identical logic. | QML files | Could be a QML function or JS helper; currently changes must be made in two places |
| A3 | `AppImageSortFilterModel::lessThan()` calls `sourceModel()->data(...)` with raw `int` role values matching `AppImageListModel` enum values, hard-coded (e.g. `AppSizeRole = Qt::UserRole + 7`). If enum ever shifts, proxy silently sorts wrong. | `appimagesortfiltermodel.cpp` | Enum values should be referenced symbolically, not as integers |

---

## Part 2 — Comment Improvements

Additions only where **WHY is non-obvious**. No docstrings on self-explanatory getters/setters.

### `src/core/appimageinfo.h`
- Above `AppImageInfo` struct: note that `description` is AppStream XML content, falls back to `comment` (.desktop `Comment=`)
- Above `isNewerVersion`: note that equal versions return false (not "newer"), so it is strictly "remote is strictly newer"
- Above `kCacheVersion` (in appimagecache.cpp): document the bump requirement when adding persisted fields

### `src/core/appimagecache.cpp`
- Above `kCacheVersion`: note current value is 3; explain that bumping invalidates all existing entries (intentional — avoids migration complexity) and list what changed at each version boundary
- In `load()`: brief note on why we check `mtime` — to detect in-place file replacement without path change

### `src/core/appimagereader.h`
- Update existing class comment to document the two read paths (`libappimage` preferred, `squashfuse` fallback) and when each is active

### `src/core/githubreleasechecker.h`
- Document the 4 signal outcomes:
  - `updateAvailable` — remote version strictly newer AND GitHub returned valid JSON
  - `upToDate` — remote version ≤ local
  - `networkFailed` — `QNetworkReply::error() != NoError`
  - `failed` — parse / API error (bad JSON, missing `tag_name`, malformed updateInfo)

### `src/core/updatedaemon.cpp`
- Above the inline GitHub check block: note that this intentionally does not reuse `GitHubReleaseChecker` because the daemon needs to batch-notify via `KNotification` per-app rather than per-signal (or: mark it as a known DRY violation to be resolved)
- Above `intervalForFrequency()`: map enum values to labels (0=Never, 1=Daily, 2=Weekly, 3=Monthly, 4=Custom)

### `src/gui/appimagebackend.h`
- Above class: single line — "Owns state + async ops for one open AppImage. One instance per `AppImageWindow`."
- Above `iconProvider` assignment in `.cpp`: explain `setParent(backend)` — provider must outlive QML engine teardown

### `src/gui/appimagelistmodel.cpp`
- Above `m_generation` usage in `loadMetadataForRow`: one-line note explaining the generation counter — stale futures from a previous `scan()` compare generation and self-discard
- Above `checkNewDownloads()`: explain the D-Bus skip — if `UpdateDaemon` is running it already sent the notification; avoids duplicates
- Above `finishOneUpdateCheck`: explain the three-counter design (`m_pendingUpdateChecks`, `m_updatesFoundInCheck`, `m_networkFailedChecks`)

### `src/gui/corpsemodel.h`
- Already has the `QML_ELEMENT` workaround note — keep as-is

### `src/gui/dashboardwindow.cpp`
- Already has the FIFO cleanup comment and disconnect-via-strings note — keep as-is

### `src/main.cpp`
- Above `KDBusService(Multiple)`: explain that `Multiple` is intentional — Dolphin can open several AppImages simultaneously; `Unique` would silently discard second opens

### `qml/DashboardWindow.qml`
- Above role constants block: add one line — "Mirror of AppImageListModel::Roles enum; update if enum shifts"
- Above the `Connections { target: listModel }` for update check: brief note on the three dialog cases

### `qml/UninstallDialog.qml`
- Above `_corpseRevision`: explain the binding-invalidation workaround — `_totalText` is a computed binding; bumping `_corpseRevision` forces re-evaluation when corpse check states change without full model reset

### `CMakeLists.txt` (root)
- Above Qt6 components list: one line per non-obvious component (Concurrent → worker threads, DBus → daemon registration)
- Above KF6 components list: same (KIO → file install/trash ops, Crash → DrKonqi integration)
- Above libappimage block: note it is optional but strongly recommended — without it, AppImage extraction falls back to `squashfuse` subprocess

---

## Files to modify

**C++ comments:**
- `src/core/appimageinfo.h`
- `src/core/appimagecache.cpp`
- `src/core/appimagereader.h`
- `src/core/githubreleasechecker.h`
- `src/core/updatedaemon.cpp`
- `src/gui/appimagebackend.h` + `.cpp`
- `src/gui/appimagelistmodel.cpp`
- `src/main.cpp`

**QML comments:**
- `qml/DashboardWindow.qml`
- `qml/UninstallDialog.qml`

**Build:**
- `CMakeLists.txt`

**Code fixes** (if approved — separate from comment pass):
- B2: disconnect/re-guard `m_updateCheckTimer` connection in `checkForUpdates()`
- B3: add early exit after `QFile::remove()` failure in `downloadUpdate()`
- P1: make the progress-parse regex `static const`
- R1: log `mkpath()` failures (3 sites)
- R2: set `QNetworkReply` transfer timeout on the request in `GitHubReleaseChecker`

## Verification
1. `cmake --build --preset dev` → zero errors after every file touched
2. Spot-check: comments render correctly in clangd hover (no stray `//` in odd places)
3. Dashboard and manage window still function normally
