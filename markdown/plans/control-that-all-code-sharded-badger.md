# Bug Fixes — Chip ColorSet Errors + File Picker

## Context

Running `appimagemanager --dashboard` produces two categories of runtime errors:

1. **ColorSet undefined errors** on all three chips in `DashboardWindow.qml`:  
   `Unable to assign [undefined] to ColorSet` — `Kirigami.Theme.Positive` and `Kirigami.Theme.Neutral` are enum values that resolve to `undefined` inside QML delegate contexts. The `Kirigami.Theme` attached property exposes color values (e.g. `positiveBackgroundColor`) but not the `ColorSet` enum in all scopes.

2. **Broken file picker** in `SettingsDialog.qml`:  
   `Qt.labs.platform.FolderDialog` requires `QApplication` (Qt Widgets). `main.cpp` uses `QGuiApplication`. The native dialog silently fails at runtime.

Both were introduced while implementing previous features. The original file picker used `QtQuick.Dialogs.FolderDialog` which is compatible with `QGuiApplication`.

---

## Fix 1 — Chip ColorSet (`qml/DashboardWindow.qml`)

**Root cause**: `Kirigami.Theme.Positive` / `Kirigami.Theme.Neutral` are `Q_ENUM` values. Inside a delegate's QML scope, the `Kirigami.Theme` attached property exposes *color* properties but not the enum itself. Assigning them as `colorSet` fails with `[undefined]`.

**Fix**: Remove all `Kirigami.Theme.colorSet` / `Kirigami.Theme.inherit` overrides from the three chips. Use conditional `palette.button` / `palette.buttonText` for the version chip's "update available" green state only. The size and shortcut chips had `colorSet` only for hover tint — `Kirigami.Chip` handles hover internally; no override needed.

### Version chip — lines 330–333, replace:
```qml
// REMOVE:
Kirigami.Theme.inherit: false
Kirigami.Theme.colorSet: model.updateAvailable
    ? Kirigami.Theme.Positive
    : (delegate.hovered || delegate.highlighted ? Kirigami.Theme.Neutral : Kirigami.Theme.Window)

// ADD:
palette.button: model.updateAvailable ? Kirigami.Theme.positiveBackgroundColor : undefined
palette.buttonText: model.updateAvailable ? Kirigami.Theme.positiveTextColor : undefined
```

### Size chip — lines 382–383, remove entirely:
```qml
Kirigami.Theme.inherit: false
Kirigami.Theme.colorSet: (delegate.hovered || delegate.highlighted) ? Kirigami.Theme.Neutral : Kirigami.Theme.Window
```

### Shortcut chip — lines 408–409, remove entirely:
```qml
Kirigami.Theme.inherit: false
Kirigami.Theme.colorSet: (delegate.hovered || delegate.highlighted) ? Kirigami.Theme.Neutral : Kirigami.Theme.Window
```

---

## Fix 2 — File Picker (`qml/SettingsDialog.qml`)

**Root cause**: `Qt.labs.platform` requires Qt Widgets backend. `main.cpp` uses `QGuiApplication`, not `QApplication`. No `Qt6::Widgets` in CMakeLists.

**Fix**: Revert to `QtQuick.Dialogs.FolderDialog`. Compatible with `QGuiApplication`; provides native KDE file picker on Plasma via `xdg-desktop-portal`. API difference: property is `selectedFolder` (not `folder`).

```qml
// REMOVE:
import Qt.labs.platform as Platform
...
Platform.FolderDialog {
    id: folderDialog
    onAccepted: {
        var path = folder.toString()
        ...
    }
}

// ADD:
import QtQuick.Dialogs
...
FolderDialog {
    id: folderDialog
    onAccepted: {
        var path = selectedFolder.toString()
        ...
    }
}
```

No CMakeLists change required. `Qt6::Quick` already linked; `QtQuick.Dialogs` ships with it.

---

## Files to Modify

| File | Change |
|---|---|
| `qml/DashboardWindow.qml` | Remove 3× colorSet overrides; add conditional palette on version chip |
| `qml/SettingsDialog.qml` | Replace `Qt.labs.platform.FolderDialog` → `QtQuick.Dialogs.FolderDialog`; `folder` → `selectedFolder` |

No C++ changes. No CMakeLists changes.

---

## Verification

```bash
cmake --build --preset dev   # zero errors

QT_LOGGING_RULES="appimagemanager=true" appimagemanager --dashboard
# No "Unable to assign [undefined] to ColorSet" in output
# No "No native FileDialog" in output

# Manual:
# 1. Hover size/shortcut chips — native hover tint, no crash
# 2. App with updateAvailable=true — version chip shows green "Available"
# 3. Settings → Browse → Plasma/portal file picker opens (not Qt custom UI)
# 4. Select valid folder → saved, no error banner
# 5. Select invalid path → error InlineMessage appears
```
