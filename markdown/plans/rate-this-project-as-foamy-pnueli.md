# Audit Plan: Bugs, Performance, Stability

## Context

Full audit against KDE Plasma development standards. Three parallel Explore agents covered core C++, GUI C++, and QML/tests. Many agent findings were false positives (verified below). Plan contains only confirmed real issues.

---

## Verified False Positives (not fixing)

- AppImageWindow "memory leak" — `s_instances.remove` + `deleteLater` already called on all QML load failure paths
- `dirSize()` symlink loop — already uses `QDir::NoSymLinks` at line 29
- `binReply` ownership — `m_nam` parents replies; context-connected lambdas auto-disconnect on `this` destroy
- Icon provider key collision — each `AppImageWindow`/`AppImageBackend` owns its own `AppImageIconProvider`
- UninstallDialog null dereference — `corpseModel` already guarded in `_totalText` binding
- `AppImageListModel` generation check position — check IS before path lookup (line 323 before 326)
- ManageWindow backend null — always set as context property before engine loads

---

## Issues to Fix (confirmed, ordered by priority)

### 1. `qml/AboutDialog.qml` — XHR stuck-spinner bug (MEDIUM)

`fetchContributors()` creates an XHR with no `onerror`, no `ontimeout`, and `xhr.timeout` unset. If GitHub API is unreachable or times out mid-request, `isLoadingContributors` stays `true` forever — spinner never hides, contributors section never renders.

**Fix:** Set `xhr.timeout = 8000` and wire both `xhr.onerror` and `xhr.ontimeout` to set `isLoadingContributors = false` and fall back to `contributors = fallbackContributors`.

---

### 2. `src/core/updatedaemon.cpp:103-106` — Timer fires on "Never" frequency (LOW-MEDIUM)

`m_timer->start()` is called unconditionally at line 106. When `updateFrequency == 0` ("Never"), `intervalForFrequency(0, ...)` falls through to `default: return hours(24)`. Result: timer wakes the daemon process every 24 hours, calls `checkUpdates()`, which returns immediately. Wastes one wakeup per day doing nothing.

**Fix:** Wrap `m_timer->start()` in `if (freq != 0)`. The existing `updateFrequencyChanged` handler already calls `m_timer->start()` only when `isActive()` — make that consistent.

---

### 3. `src/gui/appimageiconprovider.cpp:43` — Write lock serializes all concurrent icon renders (MEDIUM/Perf)

`requestPixmap()` holds a single `QWriteLocker` for the full function body. During a cold-cache dashboard scan, every worker thread calling `requestPixmap` blocks behind this lock — including expensive SVG rendering. The comment says "QCache::object mutates LRU" which is true, but `m_icons` map reads don't need a write lock.

**Fix:** Split into two locks:
- `QReadWriteLock m_iconsLock` — `QReadLocker` for `m_icons.constFind()` in `requestPixmap`; `QWriteLocker` in `setIconData` when inserting
- `QMutex m_cacheMutex` — exclusive for all `m_renderCache` operations (QCache is not RW-safe)

This lets concurrent threads read icon byte data in parallel; only cache insert/LRU ops serialize.

**Files:** `src/gui/appimageiconprovider.h`, `src/gui/appimageiconprovider.cpp`

---

### 4. `src/core/appsettings.cpp:173` — `selectedFiles().first()` without isEmpty guard (LOW)

`dialog.selectedFiles().first()` called after `dialog.exec() == QDialog::Accepted`. Standard `QFileDialog::ShowDirsOnly` + `Accepted` guarantees a non-empty list on Qt's own backends, but unusual portal backends (e.g. some Flatpak paths) could theoretically return an empty list with `Accepted`. One-liner guard eliminates the potential crash.

**Fix:** Change `QString dir = dialog.selectedFiles().first();` to:
```cpp
const QStringList selected = dialog.selectedFiles();
if (selected.isEmpty()) return;
const QString dir = selected.first();
```

**File:** `src/core/appsettings.cpp`

---

### 5. `qml/DashboardWindow.qml:256-259` — Layout properties re-evaluate on every selection change (LOW/Perf)

`Layout.maximumWidth` and `Layout.fillWidth` on the left pane both compute `listView.currentIndex < 0` inline. Every single row selection (arrow key navigation, mouse click) triggers two QML property evaluations and a full Qt Layouts pass. Extract to a cached property.

**Fix:** Add `property bool hasSelection: listView.currentIndex >= 0` near the top of the `RowLayout` or `SplitView` and reference it in both bindings.

**File:** `qml/DashboardWindow.qml`

---

### 6. `src/gui/appimageupdate.cpp` — `QFile::remove` permanently deletes old AppImage during update (MEDIUM)

Both zsync2 path (line ~203) and HTTP fallback path permanently delete the old AppImage with `QFile::remove(filePath)` before renaming the new version in. This is inconsistent with the project convention of `KIO::trash()` for user files.

**Fix:** Rename old file to `filePath + ".bak"` before the swap. On success, delete the `.bak`. On swap failure, rename `.bak` back to the original path.

```cpp
const QString bakFile = filePath + QStringLiteral(".bak");
if (!QFile::rename(filePath, bakFile)) { ... emit failure ... return; }
if (!QFile::rename(newFile, filePath)) {
    QFile::rename(bakFile, filePath); // restore
    ... emit failure ... return;
}
QFile::remove(bakFile);
```

This also makes the update atomic-ish: if the process crashes between rename steps, user finds `.bak` and can recover manually. Applies to both zsync2 finish handler and `startFullHttpDownload` finish handler.

**File:** `src/gui/appimageupdate.cpp`

---

## Files Modified

| File | Change |
|------|--------|
| [qml/AboutDialog.qml](qml/AboutDialog.qml) | XHR timeout + onerror/ontimeout handlers |
| [src/core/updatedaemon.cpp](src/core/updatedaemon.cpp) | Conditional timer start on freq != 0 |
| [src/gui/appimageiconprovider.h](src/gui/appimageiconprovider.h) | Split lock declarations |
| [src/gui/appimageiconprovider.cpp](src/gui/appimageiconprovider.cpp) | Split locks: iconsLock + cacheMutex |
| [src/core/appsettings.cpp](src/core/appsettings.cpp) | isEmpty guard before selectedFiles().first() |
| [qml/DashboardWindow.qml](qml/DashboardWindow.qml) | `hasSelection` cached property |
| [src/gui/appimageupdate.cpp](src/gui/appimageupdate.cpp) | Rename-to-bak swap pattern (both paths) |

---

## Verification

1. `cmake --build --preset dev` — zero errors
2. `ctest --test-dir build/dev --output-on-failure` — all green
3. AboutDialog: open, disable network (`nmcli networking off`), reopen dialog → spinner gone after 8s, fallback contributors shown; re-enable, reopen → contributors load
4. UpdateDaemon: set freq=Never in settings, restart daemon → timer never fires (verify via strace or log)
5. IconProvider: load dashboard with 20+ AppImages, check `htop` — no thread pileup on icon loading
6. AppSettings: test folder picker in portal environment (`FLATPAK_ID=test appimagemanager`)
7. DashboardWindow: rapid keyboard selection — no visible jank in layout transitions
8. Update: trigger update on a test AppImage, verify `.bak` created, then cleaned up on success; kill process mid-swap, verify `.bak` present for recovery
