# Code Simplification & Streamlining — Options

Each option is independent. Approve any subset.

---

## Option A — Mechanical C++ housekeeping (low risk, high confidence)

Four small extractions with zero functional change. Each is a copy-paste of identical code that currently exists in 2-6 places.

### A1. `findRowByPath()` helper method
**Problem:** Row lookup by file path is copy-pasted 4× in `appimagelistmodel.cpp` (lines 477, 520, 584, 601):
```cpp
int row = -1;
for (int idx = 0; idx < m_items.size(); ++idx)
    if (m_items.at(idx).filePath == filePath) { row = idx; break; }
```
**Fix:** Extract to `int AppImageListModel::findRowByPath(const QString &path) const`. Four callsites become one-liners. Bug fixes apply everywhere automatically.
**Files:** `appimagelistmodel.h`, `appimagelistmodel.cpp`

### A2. `normalizeVersion()` free function
**Problem:** Stripping leading `v`/`V` from version strings appears identically in `updatedaemon.cpp` (lines 185–189) and `appimagelistmodel.cpp` (lines 474–475, 484–485). The `updatedaemon.cpp` also has `isNewerVersion()` (lines 37–55) that the list model never uses — it just compares with `!=`.
**Fix:** Add `static QString normalizeVersion(const QString &v)` at file scope in both files (or a shared header), and use `isNewerVersion()` from updatedaemon in the list model instead of `!=` equality. This means "v1.0" == "1.0" comparisons stop producing false update alerts.
**Files:** `appimagelistmodel.cpp`, `updatedaemon.cpp`

### A3. AppImage filter constant
**Problem:** `{ QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") }` is written 6× across `updatedaemon.cpp` (lines 86, 102, 140), `appimagelistmodel.cpp` (lines 164, 658, 676), and `appimagemanager.cpp` (line 140).
**Fix:** Add `static const QStringList kAppImageFilters = { ... };` to `appimageinfo.h` (zero-cost, already included everywhere). Six sites reference one definition.
**Files:** `appimageinfo.h`, 3× `.cpp` files (mechanical find-replace)

### A4. Remove dead `ext` variable
**Problem:** `appimagereader.cpp` line 265 declares `QString ext;`, passes it to `readFileFromAppImage()` but never reads the result.
**Fix:** Remove the variable; the call signature already discards the ext output at that callsite. One-line deletion.
**Files:** `appimagereader.cpp`

### A5. KNotification helper
**Problem:** The 3-line `KNotification` creation pattern (`new KNotification` → `setTitle` → `setText` → `sendEvent`) repeats 7× across `updatedaemon.cpp` and `appimagelistmodel.cpp`.
**Fix:** Add a private static helper:
```cpp
static void sendNotification(QObject *parent, const QString &eventId,
                             const QString &title, const QString &text,
                             const QString &iconName = {});
```
**Files:** `appimagelistmodel.h/.cpp`, optionally `updatedaemon.h/.cpp`

---

## Option B — Consolidate GitHub update check logic (medium risk)

**Problem:** Both `UpdateDaemon` (lines 172–213) and `AppImageListModel` (lines 453–506) independently parse `gh-releases-zsync|owner|repo|...`, build the GitHub API URL, set User-Agent, parse the JSON, strip the `v` prefix, and compare versions. This is ~50 lines of near-identical code in two files. A bug fix in one doesn't fix the other (already happened: list model uses `!=` equality, daemon uses proper `isNewerVersion()`).

**Fix:** Extract a `GitHubReleaseChecker` class to `src/core/`:
```cpp
class GitHubReleaseChecker : public QObject {
    Q_OBJECT
public:
    void check(const QString &updateInfo, const QString &currentVersion,
               QNetworkAccessManager *nam);
Q_SIGNALS:
    void updateAvailable(const QString &newVersion, const QString &zsyncUrl);
    void upToDate();
    void failed(const QString &error);
};
```
Both callers construct one, connect signals, call `check()`. The 50 lines of duplication collapse to 5 lines per callsite.

**Risk:** Medium — touches both daemon and list model. Purely mechanical; no logic change.
**Files:** New `src/core/githubreleasechecker.h/.cpp`, `appimagelistmodel.cpp`, `updatedaemon.cpp`, `src/CMakeLists.txt`

---

## Option C — Simplify `checkForUpdates()` (low risk, readability only)

**Problem:** `AppImageListModel::checkForUpdates()` is 150 lines with two deeply nested network-reply lambdas inside a loop. Hard to read, hard to modify.

**Fix:** Extract two private methods:
- `void checkGitHubUpdate(int row)` — handles `gh-releases-zsync|` path
- `void checkZsyncUpdate(int row)` — handles `zsync|` path

`checkForUpdates()` becomes a short loop that dispatches. No functional change, no new classes.
**Files:** `appimagelistmodel.h`, `appimagelistmodel.cpp`

---

## Option D — Remove `currentItem` snapshot pattern in Dashboard (medium risk, maintainability)

**Problem:** `DashboardWindow.qml`'s `refreshCurrentItemAt()` (lines 47–69) copies 20 fields from the model into a JS object `currentItem`. This is fragile:
- Adding a new role requires updating the function AND every binding that uses `currentItem`
- If `dataChanged` fires for rows outside the visible range, the snapshot goes stale
- 20 `proxyModel.data(midx, roleXxx)` calls on every selection change

**Fix:** Remove `currentItem` entirely. Replace all `root.currentItem.xxx` references in the detail pane with direct `proxyModel.data(proxyModel.index(listView.currentIndex, 0), roleXxx)` bindings. Qt's binding system then tracks exactly which data each expression depends on — only affected bindings re-evaluate on `dataChanged`.

**Tradeoff:** The right pane's bindings become slightly more verbose (`proxyModel.data(...)` vs `root.currentItem.xxx`), but correctness is guaranteed by the engine rather than manual wiring. Adding a new role = just use it in the binding, no function to update.
**Files:** `qml/DashboardWindow.qml` only

---

## Option E — QML drop-area deduplication (low risk, correctness)

**Problem:** `DashboardWindow.qml` contains 3 identical drop handlers:
```qml
onDropped: (drop) => {
    for (const url of drop.urls) {
        if (url.toString().toLowerCase().endsWith(".appimage"))
            dashboardController.installFromPath(url)
    }
}
```
Lines 220–229 (empty state), 458–467 (footer), 765–770 (global overlay). If the install logic ever changes, all three must be updated simultaneously.

**Fix:** Extract `function handleDrop(drop)` at the root level, call from all three `onDropped` handlers. 3 identical blocks → 3 one-liner calls.
**Files:** `qml/DashboardWindow.qml` only

---

## Option F — Remove dead `ActionGroup` in Dashboard (trivial)

**Problem:** `DashboardWindow.qml` line 21:
```qml
Controls.ActionGroup { id: sortActionGroup }
```
All sort actions use `checked: proxyModel.sortRole === X` which already enforces mutual exclusivity via data binding. The `ActionGroup` adds a second exclusivity mechanism that conflicts — when the binding flips `checked`, the ActionGroup may fight the model's state.

**Fix:** Delete lines 21–23 and remove `Controls.ActionGroup.group: sortActionGroup` from all 4 sort actions (lines 150, 157, 163, 169). 7-line deletion.
**Files:** `qml/DashboardWindow.qml`

---

## Option G — `UninstallDialog._totalText` computed binding (low risk)

**Problem:** `_totalText` is a `property string` updated by calling `_updateTotal()` from 4 separate `Connections` handlers. This imperative wiring is fragile — if a new event path is added, the total can go stale.

**Fix:** Replace with a `readonly property` computed binding:
```qml
readonly property string _totalText: {
    if (!backend || !backend.metadataLoaded) return ""
    const corpseBytes = corpseModel ? corpseModel.checkedSize() : 0
    const total = corpseBytes + (_appChecked ? backend.appSize : 0)
    return i18n("Selected for removal: %1", backend.formatBytes(total))
}
```
Qt re-evaluates automatically whenever `checkedSize()`, `_appChecked`, or `backend.appSize` change. Remove `_updateTotal()` function and its 4 Connections callsites entirely.

**Caveat:** `checkedSize()` is a `Q_INVOKABLE`, not a `Q_PROPERTY`, so Qt can't track it automatically. Need to ensure `corpseModel` emits `dataChanged` on check toggle (it already does per CLAUDE.md). The binding works because the `Connections { target: corpseModel; function onDataChanged() }` already exists and will invalidate the binding.

Actually cleaner: keep the function, but replace the 4 Connections with a single computed property bound to `backend.busyChanged` + `corpseModel.dataChanged`. Let the existing signals drive one computed property instead of calling a function.
**Files:** `qml/UninstallDialog.qml`

---

## Option H — Download monitoring duplication (larger refactor, optional)

**Problem:** Both `UpdateDaemon` (lines 80–132) and `AppImageListModel` (lines 651–704) watch `~/Downloads` for new `.AppImage` files, fire KNotifications with a "Manage" action, and track `m_knownDownloads`. ~50 lines of near-identical code.

**Note:** These run in different processes (daemon vs. dashboard), so if both are running simultaneously, the user gets **two** notifications for the same downloaded file. This is a real bug, not just a code smell.

**Fix:** Either:
- (Simple) Add a D-Bus check: before the dashboard fires a notification, check if the daemon is running (it registers on D-Bus). If daemon is active, dashboard skips the notification.
- (Full) Extract `DownloadMonitor` class to core, used by both. One source of truth.

The simple fix is ~5 lines. The full fix is ~80 lines of new code. Recommend the simple fix first.
**Files (simple):** `appimagelistmodel.cpp` — check `QDBusInterface("org.kde.appimagemanager.daemon", ...)` before notifying

---

## Priority Summary

| Option | Risk | Lines changed | Recommend |
|--------|------|---------------|-----------|
| A — C++ housekeeping | Low | ~60 | Yes — all 5 sub-items |
| B — GitHub checker class | Medium | ~150 | Yes if B bothers you |
| C — Split checkForUpdates | Low | ~40 | Yes — readability win |
| D — Remove currentItem snapshot | Medium | ~80 | Optional — correct but verbose |
| E — Drop area function | Low | ~15 | Yes — trivial |
| F — Remove ActionGroup | Low | ~7 | Yes — correctness fix |
| G — Computed _totalText | Low | ~20 | Yes |
| H — Download double-notify fix | Low (simple) | ~5 | Yes — real bug |
