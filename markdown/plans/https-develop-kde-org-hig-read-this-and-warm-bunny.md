# KDE HIG Gap Analysis — AppImage Manager

## Context

Audit of AppImage Manager against KDE HIG (https://develop.kde.org/hig/).  
User decisions: skip empty-state PlaceholderMessage (item 3), ManageWindow fixed size is intentional, skip low-priority items 10–13.

---

## Actionable Gaps (in implementation order)

### 🔴 High

**1. Wrong icon for "Remove" / trash action**
- Files: `qml/DashboardWindow.qml` (action bar), `qml/UninstallDialog.qml`
- Problem: `icon.name: "edit-delete"` = red permanent-deletion icon. Operation is `KIO::trash()` (reversible). HIG: use `trash-empty` (black) for move-to-trash.
- Fix: `"edit-delete"` → `"trash-empty"` on the action bar Remove button and UninstallDialog confirm button.

**2. Search placeholder ends with ellipsis**
- File: `qml/DashboardWindow.qml` (SearchField placeholderText)
- Problem: `i18n("Search…")` — HIG explicitly prohibits ellipsis in placeholder text.
- Fix: `i18n("Search")`.

**4. ManageWindow uses plain `ApplicationWindow` not `Kirigami.ApplicationWindow`**
- File: `qml/ManageWindow.qml`
- Problem: Root is QtQuick.Controls `ApplicationWindow`. Kirigami theming/action system requires `Kirigami.ApplicationWindow`.
- Fix: Change root type. Remove manual window flags Kirigami handles. Keep fixed size (intentional).

---

### 🟡 Medium

**5. Dashboard window size not remembered**
- Files: `src/gui/dashboardwindow.cpp`, `qml/DashboardWindow.qml`
- Problem: Always opens at 50×30 gridUnits. HIG: remember window size/position between sessions.
- Fix: Call `KWindowConfig::saveWindowSize(m_window, config)` on close, `restoreWindowSize(m_window, config)` before show. Use `KSharedConfig::openConfig()` group `"DashboardWindow"`.

**7. `invalidateFilter()` deprecated**
- File: `src/gui/appimagesortfiltermodel.cpp` line 20
- Problem: Qt 6.9 deprecated `invalidateFilter()`, emitting a compiler warning. Use `beginFilterChange()` / `endFilterChange()` instead.
- Fix:
  ```cpp
  beginFilterChange();
  // (no body needed — invalidation is implicit)
  endFilterChange();
  ```

**8. Notifications fired while main window is visible**
- File: `src/gui/appimagelistmodel.cpp` (download watcher lambda)
- Problem: HIG says don't send notifications when app's main window is in the foreground.
- Fix: In the `appImageFound` lambda, check `QDBusConnection` for daemon (already done), then also suppress if the dashboard window is currently active. Add a static/weak check via `DashboardWindow` or expose a signal. Simplest: check `QGuiApplication::focusWindow() != nullptr && QGuiApplication::applicationState() == Qt::ApplicationActive` before firing.

**9. No inline trailing actions on list items**
- File: `qml/DashboardWindow.qml` delegate contentItem RowLayout
- Problem: HIG recommends 0–2 trailing icon-only action buttons for obvious actions. Currently requires click-to-select then action bar.
- Fix: Add a trailing `Controls.ToolButton` (icon: `"media-playback-start"`, tooltip: i18n("Launch")) visible on hover (`opacity: delegateRoot.hovered ? 1 : 0`) inside the delegate RowLayout, before the update icon. Optionally add a trash button too.

---

## What stays unchanged (already correct)

- Fixed ManageWindow size — intentional
- Empty installed-apps state — skip for now
- Low-priority items (chips, RTL, screen reader, FormCard) — skip

## Verification

After implementing:
1. Build: `cmake --build --preset dev` — zero errors, no `invalidateFilter` warning
2. Dashboard Remove button shows black trash icon, not red X
3. UninstallDialog confirm button shows black trash icon
4. Dashboard search field shows "Search" placeholder (no ellipsis)
5. ManageWindow opens via `Kirigami.ApplicationWindow` — theme consistent
6. Close and reopen dashboard — same size restored
7. Copy `.AppImage` to `~/Downloads` while dashboard focused — no notification fires
8. Hover a list item — Launch button appears
