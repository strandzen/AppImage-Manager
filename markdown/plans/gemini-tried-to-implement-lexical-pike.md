# Three Dashboard Fixes

## Context
Three independent issues in the dashboard UI. Description feature is now working (cache bug fixed). These are follow-up polish items.

---

## Fix 1 — Chip/text overflow clipping in details pane

**Problem:** `RowLayout { id: chipsRow }` (DashboardWindow.qml:478) doesn't wrap. When chips are too wide for the pane (narrow window), they overflow and clip.

**Fix:** Replace `RowLayout` with `Flow` for the chips container.

### `qml/DashboardWindow.qml` lines 478–504
```qml
// BEFORE
RowLayout {
    id: chipsRow
    Layout.alignment: Qt.AlignHCenter
    spacing: Kirigami.Units.smallSpacing
    visible: root.currentItem.metadataLoaded ?? false

// AFTER
Flow {
    id: chipsRow
    Layout.fillWidth: true
    spacing: Kirigami.Units.smallSpacing
    visible: root.currentItem.metadataLoaded ?? false
```
Chips themselves unchanged — `Flow` auto-wraps them.

Also update buttons RowLayout (line 526–529): remove `Layout.preferredWidth: chipsRow.implicitWidth` (no longer meaningful with Flow), add `Layout.fillWidth: true` to the RowLayout instead.

---

## Fix 2 — Update/Remove button bottom margin

**Problem:** Buttons RowLayout has `Layout.bottomMargin: Kirigami.Units.gridUnit` AND parent ColumnLayout has `anchors.margins: Kirigami.Units.gridUnit` → total ≈ 2×gridUnit from bottom. Drag box has `anchors.bottomMargin: 2`. User wants both panels' bottom edges to feel consistent.

**Fix:** Override bottom anchor on the details ColumnLayout, remove extra Layout.bottomMargin from buttons.

### `qml/DashboardWindow.qml` line 459 — add bottomMargin override
```qml
ColumnLayout {
    anchors.fill: parent
    anchors.margins: Kirigami.Units.gridUnit
    anchors.bottomMargin: 2        // ← add this line
    spacing: Kirigami.Units.largeSpacing
```

### `qml/DashboardWindow.qml` lines 526–529 — buttons RowLayout
```qml
// BEFORE
RowLayout {
    Layout.alignment: Qt.AlignHCenter
    Layout.bottomMargin: Kirigami.Units.gridUnit
    Layout.preferredWidth: chipsRow.implicitWidth

// AFTER
RowLayout {
    Layout.alignment: Qt.AlignHCenter
    Layout.fillWidth: true
```

---

## Fix 3 — Downloads folder watcher (dashboard-side notification)

**Root cause:** `UpdateDaemon` already implements the notification logic correctly (updatedaemon.cpp:80–132) — watches `QStandardPaths::DownloadLocation`, sends `KNotification` with "Manage" action that runs `appimagemanager <path>`. BUT the daemon runs as a **separate process** launched via autostart at login. When the user opens the dashboard, the daemon is not necessarily running.

**Fix:** Port the exact same notification logic into `AppImageListModel` (which is always running when the dashboard is open). Reuse the **existing** `AppSettings.watchDownloads` setting and its existing UI toggle ("Notify when an AppImage is downloaded" in SettingsDialog.qml — no QML changes needed).

### `src/gui/appimagelistmodel.h`
Add private member:
```cpp
QSet<QString> m_knownDownloads;
```
(Already has `#include <QSet>` via Qt headers, `QFileSystemWatcher m_watcher` at line 117.)

### `src/gui/appimagelistmodel.cpp` — constructor (lines 25–42)

1. Change existing `directoryChanged` connection to pass the path and route by directory:
```cpp
connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
        this, [this](const QString &changedPath) {
            const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            if (changedPath == dlDir)
                checkNewDownloads();
            else
                m_refreshTimer.start();
        });
```

2. Add connection to react when the setting is toggled:
```cpp
connect(AppSettings::instance(), &AppSettings::watchDownloadsChanged,
        this, [this]() { updateDownloadWatcher(); });
```

3. Call `updateDownloadWatcher()` at end of constructor to set up on start.

### `src/gui/appimagelistmodel.cpp` — two new private methods

**`updateDownloadWatcher()`** — add/remove the downloads dir from `m_watcher` and populate `m_knownDownloads`:
```cpp
void AppImageListModel::updateDownloadWatcher()
{
    const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dlDir.isEmpty()) return;

    if (AppSettings::instance()->watchDownloads()) {
        const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
        m_knownDownloads.clear();
        for (const QFileInfo &fi : QDir(dlDir).entryInfoList(filters, QDir::Files))
            m_knownDownloads.insert(fi.absoluteFilePath());
        if (!m_watcher.directories().contains(dlDir))
            m_watcher.addPath(dlDir);
    } else {
        if (m_watcher.directories().contains(dlDir))
            m_watcher.removePath(dlDir);
    }
}
```

**`checkNewDownloads()`** — mirrors `UpdateDaemon::onDownloadDirChanged()`:
```cpp
void AppImageListModel::checkNewDownloads()
{
    if (!AppSettings::instance()->watchDownloads()) return;
    const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QStringList filters = { QStringLiteral("*.AppImage"), QStringLiteral("*.appimage") };
    const QFileInfoList entries = QDir(dlDir).entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : entries) {
        const QString path = fi.absoluteFilePath();
        if (m_knownDownloads.contains(path)) continue;
        m_knownDownloads.insert(path);

        QString displayName = AppImageReader::cleanName(fi.fileName());
        displayName.remove(QStringLiteral(".AppImage"), Qt::CaseInsensitive);
        displayName.remove(QRegularExpression(QStringLiteral(R"(\s+\d[\d.]*$)")));
        displayName = displayName.trimmed();

        auto *n = new KNotification(QStringLiteral("downloaded"), KNotification::Persistent, this);
        n->setTitle(i18n("%1 downloaded", displayName));
        n->setIconName(QStringLiteral("application-x-executable"));
        auto *action = n->addAction(i18n("Manage"));
        connect(action, &KNotificationAction::activated, this, [path]() {
            QProcess::startDetached(QStringLiteral("appimagemanager"), { path });
        });
        n->sendEvent();
    }

    QSet<QString> current;
    for (const QFileInfo &fi : entries)
        current.insert(fi.absoluteFilePath());
    m_knownDownloads.intersect(current);
}
```

### `src/gui/appimagelistmodel.h` — declare the two new methods
```cpp
private:
    void updateDownloadWatcher();
    void checkNewDownloads();
```

### Includes to add to `appimagelistmodel.cpp` (if not already present)
- `#include <QStandardPaths>` — needed for `DownloadLocation`

(`KNotification`, `QProcess`, `QRegularExpression` already included per lines 1–23.)

---

## Critical Files
- `qml/DashboardWindow.qml` — fixes 1 and 2
- `src/gui/appimagelistmodel.h` — fix 3 (new members + method declarations)
- `src/gui/appimagelistmodel.cpp` — fix 3 (constructor, two new methods)

*(No changes to appsettings or SettingsDialog — existing `watchDownloads` setting and its "Notify when an AppImage is downloaded" toggle are correct.)*

## Verification
```bash
cmake --build --preset dev
sudo cmake --install build/dev
appimagemanager --dashboard
```
- Narrow window → chips wrap to second line, no horizontal clip
- Update/Remove buttons sit close to the bottom edge (≈2px), matching drag box
- Settings → "Notify when an AppImage is downloaded" is on (default)
- Copy/move a .AppImage into ~/Downloads → KNotification appears within seconds
- Click "Manage" in notification → ManageWindow opens for that AppImage
- Toggle setting off → no more notifications; toggle on → resumes
