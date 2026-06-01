# AppImageManager: Improvement Implementation Plan

## Context
Senior KDE dev analysis identified fixes across performance, architecture, maintainability,
reliability, KDE Store compliance, and bundle size. This plan implements everything except
the QWidgets→FolderDialog migration (user to decide separately after explanation).

Two analysis findings turned out to be false positives after reading actual code:
- `QRegularExpression` in appimageupdate.cpp:189 is ALREADY `static const` — no change needed
- `watcher->deleteLater()` IS called before early-return paths — no watcher leak

One analysis recommendation withdrawn: "Remove AppSettings proxy class" — AppSettings is NOT
pure boilerplate. It owns path validation (mkpath + applicationsPathError signal), version
read-only properties (qtVersion, kdeFrameworksVersion, appVersion), QFileDialog invocation,
and copyToClipboard. The KCfg generated class (Core=true) has no QML signals. AppSettings
stays. The improvement is to wire `AppImageManagerSettings` notifications to AppSettings
signals directly (eliminate manual save()+emit pairs) — deferred as low ROI.

---

## Phase 1 — Performance (appimagelistmodel.cpp)

### 1a. Specific roles in `dataChanged`

**`applyMetadata` (line 428):** Currently emits with no roles → all delegates re-evaluate.
Change to emit the full set of roles that `applyMetadata` actually mutates:

```cpp
Q_EMIT dataChanged(index(row, 0), index(row, 0), {
    IconSourceRole, DisplayNameRole, CleanNameRole, AppNameRole,
    VersionRole, FormattedSizeRole, AppSizeRole, HasDesktopLinkRole,
    MetadataLoadedRole, CategoriesRole, CommentRole, DescriptionRole,
    DeveloperNameRole, HomepageRole
});
```

**`toggleDesktopLink` (line 442):** Change to `{HasDesktopLinkRole}`.

**`onFileDirty` (line 403):** Clears `metadataLoaded = false` then re-queues a load.
This correctly invalidates everything, so emitting with no roles (= all roles) is acceptable.
No change needed here.

### 1b. O(1) `findRowByPath` — add reverse index

Add `QHash<QString, int> m_pathIndex` to `AppImageListModel` (appimagelistmodel.h:152-ish).

Maintain it at:
- Insert: `onFileCreated` — after `endInsertRows`, `m_pathIndex[path] = newRow`; rebuild on
  scan/refresh with a full pass (not on every insert — just clear+rebuild after `endInsertRows`
  for scan, or single-insert update for KDirWatch creates).
- Remove: `onFileDeleted` — remove key, then decrement all indices > removed row (or rebuild).
  Simpler: always rebuild after any structural change. `m_pathIndex` is correct at all times
  since structural changes (insert/delete) already go through single entry-points.
- `findRowByPath` becomes: `return m_pathIndex.value(path, -1);`

Rebuild helper:
```cpp
void AppImageListModel::rebuildPathIndex() {
    m_pathIndex.clear();
    for (int i = 0; i < m_items.size(); ++i)
        m_pathIndex[m_items.at(i).filePath] = i;
}
```
Call after scan populate, after KDirWatch inserts, after KDirWatch removes.

Files: `src/gui/appimagelistmodel.h`, `src/gui/appimagelistmodel.cpp`

---

## Phase 2 — Architecture: GitHubReleaseChecker signal collapse

Current: 4 signals (`updateAvailable`, `upToDate`, `networkFailed`, `failed`).
All callers must connect to all 4 separately (updatedaemon.cpp, appimageupdate.cpp).

Change to: one `checkFinished(Result result, QString newVersion, QString zsyncUrl)` signal
where `Result` is an enum:

```cpp
enum class Result { UpdateAvailable, UpToDate, NetworkFailed, Failed };
Q_ENUM(Result)
Q_SIGNAL void checkFinished(Result result, const QString &newVersion, const QString &zsyncUrl);
```

Internal `emitXxx()` calls become:
```cpp
Q_EMIT checkFinished(Result::UpdateAvailable, newVersion, zsyncUrl);
Q_EMIT checkFinished(Result::UpToDate, {}, {});
Q_EMIT checkFinished(Result::NetworkFailed, {}, {});
Q_EMIT checkFinished(Result::Failed, {}, {});
```

Callers updated:
- `src/core/updatedaemon.cpp` — connects once, switches on result
- `src/gui/appimageupdate.cpp` (AppImageUpdateManager) — same

Files: `src/core/githubreleasechecker.h`, `src/core/githubreleasechecker.cpp`,
`src/core/updatedaemon.cpp`, `src/gui/appimageupdate.cpp`

---

## Phase 3 — Maintainability

### 3a. Shared cardBorderColor — extract to `qml/Theme.qml`

`DashboardWindow.qml:21-25` and `ManageWindow.qml` each define the same opacity+color math.
Create `qml/Theme.qml` as a `QtObject` singleton (via `pragma Singleton`):

```qml
pragma Singleton
import QtQuick
import org.kde.kirigami as Kirigami
QtObject {
    readonly property color cardBorderColor: AppSettings.accentBorders
        ? Kirigami.Theme.focusColor
        : Qt.rgba(Kirigami.Theme.textColor.r,
                  Kirigami.Theme.textColor.g,
                  Kirigami.Theme.textColor.b, 0.15)
}
```

Register in `src/CMakeLists.txt` qt_add_qml_module QML_FILES list.
Both DashboardWindow and ManageWindow import and use `Theme.cardBorderColor`.

Files: `qml/Theme.qml` (new), `qml/DashboardWindow.qml`, `qml/ManageWindow.qml`,
`src/CMakeLists.txt`

### 3b. data/CMakeLists.txt locale simplification

Replace custom cmake PO-merge script with:
```cmake
ki18n_install(po)
```
ECM's `ki18n_install` handles the merge, QM generation, and installation automatically.
Remove the manual `msgfmt`/loop/`configure_file` block.

Files: `data/CMakeLists.txt`

---

## Phase 4 — Reliability fixes

### 4a. Log cache failures (appimagecache.cpp:114,151)

After DB open check, add `qCWarning(AIM_LOG) << "Cache DB unavailable:" << db.lastError();`
and after `exec()` failures.

### 4b. D-Bus registerService() checks

`src/gui/dashboardwindow.cpp` and `src/core/updatedaemon.cpp` — check return bool:
```cpp
if (!QDBusConnection::sessionBus().registerService(serviceName))
    qCWarning(AIM_LOG) << "Failed to register D-Bus service:" << serviceName;
```

### 4c. startDetached() check in updatedaemon.cpp

Lines 73, 89, 101, 120 — wrap each with:
```cpp
if (!QProcess::startDetached(...))
    qCWarning(AIM_LOG) << "Failed to launch:";
```
(requestLaunch in appimagelistmodel.cpp already does this correctly at line 455-457.)

Files: `src/core/appimagecache.cpp`, `src/gui/dashboardwindow.cpp`,
`src/core/updatedaemon.cpp`

---

## Phase 5 — KDE Store compliance

### 5a. metainfo.xml additions (`data/appimagemanager.metainfo.xml`)

Add:
```xml
<url type="donation">https://ko-fi.com/strandzen</url>  <!-- or wherever -->

<kudos>
  <kudo>ModernToolkit</kudo>
  <kudo>HiDpiIcon</kudo>
  <kudo>Notifications</kudo>
</kudos>

<content_rating type="oars-1.1">
  <content_attribute id="social-info">mild</content_attribute>
  <content_attribute id="network-transactions">mild</content_attribute>
</content_rating>
```

(Confirm donation URL with user before committing.)

### 5b. D-Bus service file (`data/io.github.appimagemanager.Daemon.service`)

Create and install to `${KDE_INSTALL_DBUSSERVICEDIR}`:
```ini
[D-Bus Service]
Name=io.github.appimagemanager.Daemon
Exec=@CMAKE_INSTALL_FULL_BINDIR@/appimagemanager --daemon
```
Wire in `data/CMakeLists.txt` via `configure_file` + `install(FILES ...)`.

Files: `data/io.github.appimagemanager.Daemon.service.in` (new), `data/CMakeLists.txt`

---

## Phase 6 — Tests

### 6a. D-Bus adaptor test (`tests/tst_appimagedbusadaptor.cpp`)

- Create `AppImageListModel` + `AppImageDBusAdaptor` in test process
- Register adaptor on session bus
- Call `InstalledAppImages()` via `QDBusInterface` — verify returns a `QVariantList`
- Call `Version()` — verify matches `APPIMAGEMANAGER_VERSION_STRING`
- Verify `AllMetadataLoaded()` (pending — if we add it) or document the limitation

### 6b. Error path tests in `tests/tst_appimagecache.cpp`

- Test: store to read-only directory → returns false, logs warning, no crash
- Test: load from corrupted DB file → returns invalid AppImageInfo, no crash

Files: `tests/tst_appimagedbusadaptor.cpp` (new), `tests/tst_appimagecache.cpp`,
`tests/CMakeLists.txt`

---

## Phase 7 — Lighter: Shared icon data store

`AppImageIconProvider` currently has per-instance `QHash<QString, IconEntry>` + 32MB render cache.
With multiple engines (dashboard + manage windows), raw icon bytes are stored N times.

Extract a singleton `AppImageIconStore`:
```cpp
// Owns shared raw bytes across all engines
class AppImageIconStore {
    static AppImageIconStore &instance();
    void set(const QString &id, const QByteArray &data, const QString &ext);
    bool get(const QString &id, QByteArray &data, QString &ext) const;
    QHash<QString, IconEntry> m_icons;
    mutable QReadWriteLock    m_lock;
};
```

`AppImageIconProvider` becomes a thin per-engine wrapper:
- `setIconData` → delegates to `AppImageIconStore::instance().set()`
- `requestPixmap` → reads raw bytes from store (shared, shared lock), renders locally
  into `m_renderCache` (per-engine, no lock needed — called from engine's render thread only)

This eliminates duplicate raw icon storage across windows while keeping render caches separate
(pixmaps must not cross engine boundaries).

Files: `src/gui/appimageiconprovider.h`, `src/gui/appimageiconprovider.cpp` (new singleton store)

---

## Verification

```bash
cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure

# D-Bus service file installed
ls /usr/share/dbus-1/services/io.github.appimagemanager.Daemon.service

# Test singleton icon store: open dashboard + manage window for same AppImage,
# verify icon appears in both, check no duplicate data in memory (if tooling available)

# Validate metainfo
appstreamcli validate data/appimagemanager.metainfo.xml
```

Manual checklist: items 1–21 from CLAUDE.md still apply.
