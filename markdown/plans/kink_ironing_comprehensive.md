# Comprehensive Kink-Ironing Plan

## Context

Full audit of AppImageManager v1.5.0 for functional gaps and KDE Plasma standards compliance.
Previous session fixed: Discover page UI freeze, BusyIndicator alignment, icon fallback.
This plan addresses the remaining confirmed issues across UI, backend, and packaging.

---

## Batch 1 — Quick Wins (trivial, high signal)

### 1.1 `StartupNotify=true` missing from main desktop file

`data/io.github.strandzen.AppImageManager.desktop` — add after `Terminal=false`:
```ini
StartupNotify=true
```
Without this, KDE shows no launch feedback cursor (spinning wheel) when opening the app.

---

### 1.2 Root PKGBUILD outdated / misleading

`/home/herman/Documents/Project/AppImageManager/PKGBUILD` — currently at `pkgver=1.2.0` (3 versions stale).
This is a dev-convenience file at the repo root, not the maintained AUR package. Options:
- **Delete it** (maintained packages live under `packaging/aur/`) — recommended
- OR sync `pkgver` to 1.5.0

---

### 1.3 Missing `fallback` property on LibraryPage detail icon

`qml/LibraryPage.qml` line 499–504: The detail-pane hero icon uses `??` coalescing but misses Kirigami.Icon's `fallback:` property. When the icon source URL fails to load, Kirigami shows a broken-image state instead of the executable icon.

```qml
// Current:
Kirigami.Icon {
    source: pageRoot.displayedItem.iconSource ?? "application-x-executable"
    implicitWidth: Kirigami.Units.iconSizes.enormous
    implicitHeight: Kirigami.Units.iconSizes.enormous
    Layout.alignment: Qt.AlignHCenter
}

// Fix — add fallback:
Kirigami.Icon {
    source: pageRoot.displayedItem.iconSource ?? "application-x-executable"
    fallback: "application-x-executable"
    implicitWidth: Kirigami.Units.iconSizes.enormous
    implicitHeight: Kirigami.Units.iconSizes.enormous
    Layout.alignment: Qt.AlignHCenter
}
```

---

### 1.4 Missing Ctrl+Q / Ctrl+W keyboard shortcuts in DashboardWindow

`qml/DashboardWindow.qml` lines 312–322 — only `Ctrl+F` is defined. KDE standard is:
- `Ctrl+Q` — quit application
- `Ctrl+W` — close window (same effect here)

Add after the existing Ctrl+F shortcut:
```qml
Shortcut {
    sequence: StandardKey.Quit
    onActivated: Qt.quit()
}
Shortcut {
    sequence: StandardKey.Close
    onActivated: root.close()
}
```

---

## Batch 2 — Functional Fixes

### 2.1 `toggleDesktopLink` failure is silent

`src/gui/appimagelistmodel.cpp` lines 447–458: when `createDesktopLink()` or `removeDesktopLink()` returns false, nothing happens. User sees the toggle revert with no explanation.

`AppImageListModel` already has a `sendError()` pattern used in download failures. Add it here:

```cpp
void AppImageListModel::toggleDesktopLink(int row, bool enable)
{
    if (row < 0 || row >= m_items.size()) return;
    Item &item = m_items[row];
    const bool ok = enable
        ? AppImageManager::createDesktopLink(item.filePath, item.info)
        : AppImageManager::removeDesktopLink(item.filePath, item.info);
    if (ok) {
        item.hasDesktopLink = enable;
        Q_EMIT dataChanged(index(row, 0), index(row, 0), {HasDesktopLinkRole});
    } else {
        // Notify user; toggle revert happens automatically since hasDesktopLink didn't change
        auto *n = new KNotification(QStringLiteral("error"),
                                    KNotification::CloseOnTimeout, this);
        n->setTitle(i18n("Desktop Link Failed"));
        n->setText(enable
            ? i18n("Could not create desktop link for %1.", item.info.appName)
            : i18n("Could not remove desktop link for %1.", item.info.appName));
        n->setIconName(QStringLiteral("dialog-error"));
        n->sendEvent();
    }
}
```

Needs `#include <KNotification>` (already present in the file via other usages — verify).

---

### 2.2 Desktop file slug collision

`src/core/appimagemanager.cpp` lines 219–226: the slug is derived purely from `cleanName`. Two apps with similar names (e.g. `My App` and `my-app`) produce identical slugs → one overwrites the other's `.desktop` file silently.

Fix: append a short hash of the original file path to make the slug unique:

```cpp
// appimagemanager.cpp — desktopFilePath()
QString desktopFilePath(const AppImageInfo &info)
{
    const QString slug = info.cleanName.toLower()
                             .remove(QLatin1String(".appimage"))
                             .replace(QLatin1Char(' '), QLatin1Char('_'));
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    // Append first 6 hex chars of MD5(appId) to prevent collision between apps with similar names
    const QString suffix = QString::number(qHash(info.appId), 16).left(6);
    return appsDir + QStringLiteral("/appimage_") + slug + QLatin1Char('_') + suffix + QStringLiteral(".desktop");
}
```

Same change needed in `iconFilePath()` for icon file uniqueness.

**Note:** This changes the generated path, so existing `.desktop` files at the old slug path won't be found by `isDesktopLinkEnabled()`. On first run after upgrade, the old detection breaks. The `isDesktopLinkEnabled` function also uses `desktopFilePath()` to check existence, so it'll return false for previously-created links.

**Migration strategy:** In `isDesktopLinkEnabled()`, check both the new path AND the old path (without suffix). Remove old path as part of cleanup if found. OR: use `appId` directly instead of derived slug — `appId` is already unique (set by AppImageReader from AppStream or `.desktop` ID).

Actually, the cleaner fix is: **use `info.appId` directly** as the desktop file name, since `appId` is already unique per app (it's the reverse-domain identifier like `com.github.calibre`). If `appId` is empty, fall back to `slug + hash`.

```cpp
QString desktopFilePath(const AppImageInfo &info)
{
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    const QString key = info.appId.isEmpty()
        ? info.cleanName.toLower()
              .remove(QLatin1String(".appimage"))
              .replace(QLatin1Char(' '), QLatin1Char('_'))
              + QLatin1Char('_') + QString::number(qHash(info.cleanName + info.version), 16).left(6)
        : info.appId;
    return appsDir + QStringLiteral("/appimage_") + key + QStringLiteral(".desktop");
}
```

**Files affected:** `src/core/appimagemanager.cpp` — `desktopFilePath()` and `iconFilePath()`.

---

## Batch 3 — UI Polish

### 3.1 StorePage: missing empty/error state before initialization

`qml/StorePage.qml`: The `Kirigami.PlaceholderMessage` at line 296 only shows when `filterText !== ""` (search empty state). There's no placeholder for:
- Before `initialize()` is called (first open, before onVisibleChanged fires) — though BusyIndicator covers this
- After load completes but the model is empty (network failure + no cache)

Add a second `PlaceholderMessage` inside the list `Item`:
```qml
Kirigami.PlaceholderMessage {
    anchors.centerIn: parent
    visible: !amStoreModel.loading && listView.count === 0 && amStoreModel.filterText === ""
    icon.name: "network-offline"
    text: i18n("Could not load app catalog")
    explanation: i18n("Check your network connection and try again.")
    helpfulAction: Kirigami.Action {
        text: i18n("Retry")
        icon.name: "view-refresh"
        onTriggered: amStoreModel.sync()
    }
}
```

---

### 3.2 LibraryPage: "Show in app menu" toggle gives no feedback

`qml/LibraryPage.qml` — when the user toggles the desktop link, there's no visible confirmation. The toggle reflects the new state (or reverts on failure via fix 2.1), but a `showPassiveNotification` call would add polish:

After `proxyModel.toggleDesktopLink(...)` in the QML action, add:
```qml
showPassiveNotification(checked
    ? i18n("Added %1 to app menu", pageRoot.displayedItem.displayName)
    : i18n("Removed %1 from app menu", pageRoot.displayedItem.displayName))
```

But: the success/failure comes from the C++ side (fix 2.1 sends a KNotification on error). For the success case, a passive notification here in QML is the right place.

---

## Summary — Files Changed

| Batch | File | Change |
|-------|------|--------|
| 1.1 | `data/io.github.strandzen.AppImageManager.desktop` | Add `StartupNotify=true` |
| 1.2 | `PKGBUILD` | Delete (root-level, outdated) |
| 1.3 | `qml/LibraryPage.qml` | Add `fallback:` to detail icon |
| 1.4 | `qml/DashboardWindow.qml` | Add `Ctrl+Q` / `Ctrl+W` shortcuts |
| 2.1 | `src/gui/appimagelistmodel.cpp` | Emit KNotification on toggleDesktopLink failure |
| 2.2 | `src/core/appimagemanager.cpp` | Fix slug collision in `desktopFilePath` + `iconFilePath` |
| 3.1 | `qml/StorePage.qml` | Add network-error placeholder message |
| 3.2 | `qml/LibraryPage.qml` | Add passive notification for desktop link toggle success |

---

## Verification

```bash
cmake --build --preset dev && sudo cmake --install build/dev
```

1. Launch `appimagemanager` → spinning cursor during launch (StartupNotify)
2. Press Ctrl+Q → app quits
3. Press Ctrl+W → window closes
4. Library page: select an AppImage → detail icon shows correctly, fallback to executable icon on failure
5. Library page: toggle "Show in app menu" → passive notification appears
6. Library page: trigger a desktop link failure (e.g. read-only filesystem) → KNotification error appears
7. Discover page → no network → retry button appears after load attempt
8. `~/.local/share/applications/` → two apps with similar names each get unique `.desktop` file
9. `ls PKGBUILD` → file gone (or updated)
10. `grep StartupNotify /usr/share/applications/io.github.strandzen.AppImageManager.desktop` → present
