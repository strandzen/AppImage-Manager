# UI/UX Refinement Plan

## Context

Full audit of all five QML files. Goal: close gaps between current state and a polished Plasma-native experience. Improvements grouped by surface, ordered high→low impact.

---

## Dashboard (`DashboardWindow.qml`)

### 1. Launch button in detail pane

**Problem:** Detail pane has Update + Remove buttons but no Launch. Primary action for an installed app is missing. User must go back to Dolphin or find the app in the app menu.

**Fix:** Add Launch button (calls `proxyModel.requestLaunch(listView.currentIndex)`). Already exists as `Q_INVOKABLE` in the model. Show only when `metadataLoaded`.

### 2. Desktop link toggle in detail pane

**Problem:** `hasDesktopLink` role exists in the model and `toggleDesktopLink()` is invokable, but no UI surface exposes it. Users can't add/remove the app from the app menu from the dashboard.

**Fix:** Add a toggle button or checkbox in the detail pane action row. Label: "Show in App Menu" with `starred` / `bookmarks` icon.

### 3. "By Date" sort option

**Problem:** Sort menu has Name/Size/Category but not Date. `AddedDateRole` (index 9) is already in the model and `AppImageSortFilterModel` likely supports it. Date sort is a natural "recently added" view.

**Fix:** Add sort action `i18n("By Date")` → `proxyModel.sortRole = 3` (AddedDateRole). Verify sort model handles it.

### 4. Homepage as readable link, not just icon

**Problem:** Homepage URL shows only as a small `ToolButton` globe icon next to developer name. Easily missed. URL text itself is useful (shows domain = trust signal).

**Fix:** Show `hostname` of the URL as a clickable `Label` with `linkColor`. Keep the icon but make the label the primary target. `Qt.resolvedUrl` → `url.host`.

---

## Manage Window (`ManageWindow.qml`)

### 6. Drag instruction hint text

**Problem:** The drag-to-install UX is clever but completely undiscoverable. No text tells the user to drag the icon. Many users will not figure it out.

**Fix:** Add a small muted `Label` below the icon area: "Drag icon to install" (hidden after install). Already have `opacity` animation on the folder column — attach to same condition.

### 7. Version/size as chips instead of one label

**Problem:** `"Version: 1.2  •  Size: 42 MB"` is a dense single label. Inconsistent with dashboard chips pattern. Hard to scan.

**Fix:** Replace with two `Kirigami.Chip` elements in a `Flow`, matching dashboard style.

### 8. Replace About `OverlaySheet` with `Kirigami.Dialog`

**Problem:** `Kirigami.OverlaySheet` is deprecated in Kirigami 6 in favor of `Kirigami.Dialog`. Causes style inconsistency.

**Fix:** Port `aboutSheet` to `Kirigami.AboutDialog` or a plain `Kirigami.Dialog`. Content stays the same.

---

## Uninstall Dialog (`UninstallDialog.qml`)

### 9. Show `~/`-relative paths instead of full absolute paths

**Problem:** Corpse list shows `/home/herman/.config/SomeApp/` — hard to read. The home prefix is noise.

**Fix:** In the delegate label, replace `QStandardPaths::HomeLocation` prefix with `~`. Simple JS: `filePath.replace(Qt.resolvedUrl("~"), "~")` or pass already-shortened paths from C++. Prefer C++ side: trim in `CorpseModel` or format in delegate with `StandardPaths` constant passed from QML context.

### 10. Select All / Deselect All

**Problem:** With 5+ corpse entries, individual checkbox clicking is tedious. Power users expect batch control.

**Fix:** Add "Select All" / "Deselect All" `Controls.Button` pair above the corpse `ListView`. Call `corpseModel.setAllChecked(bool)` — add this slot to `CorpseModel`.

### 11. Collapse the confidence warning

**Problem:** The inline warning about confidence sorting is 2 lines, always visible when corpses exist. Takes up significant space for information only relevant on first use.

**Fix:** Replace with a small `(?)` `ToolButton` next to the "Select items to remove:" label. `ToolTip` shows the full explanation on hover.

---

## Storage Dialog (`StorageDialog.qml`)

### 12. "Related files" section header

**Problem:** The corpse list appears below the AppImage row with no heading. Users don't know what those directories are.

**Fix:** Add a `Kirigami.Separator` with label `i18n("Related Files")` above the `ListView` (same pattern as `Kirigami.FormData.isSection`).

### 13. Empty state for no related files

**Problem:** When there are no related files, the separator and `ListView` render nothing with no explanation. Looks like a load failure.

**Fix:** Show `Controls.Label { text: i18n("No related files found."); opacity: 0.6 }` when `corpseModel.count === 0`.

### 14. Show total size in footer

**Problem:** Footer only has a Close button. No summary of how much disk the app uses in total.

**Fix:** Add a `Controls.Label` to the left of the Close button showing `i18n("Total: %1", backend.formattedSize)` (or sum of all listed items).

---

## Settings Dialog (`SettingsDialog.qml`)

### 15. Add Close button

**Problem:** `standardButtons: Kirigami.Dialog.NoButton` — no close button. Only closable via Escape or window X. Every other dialog in the app has an explicit close action.

**Fix:** Set `standardButtons: Kirigami.Dialog.Close`.

### 16. Clarify "Show install drag box in list" label

**Problem:** Label is cryptic. New users don't know what a "drag box" is.

**Fix:** Rename to `i18n("Show drag-and-drop install zone at bottom of list")`.

---

## Cross-cutting

### 17. Keyboard shortcuts

**Problem:** No keyboard shortcuts anywhere. Standard desktop expectations: `Delete` = remove selected, `Return` = launch, `Escape` = deselect, `Ctrl+F` = focus search.

**Fix:** Add `Shortcut` items in `DashboardWindow.qml`:
- `Ctrl+F` → `searchField.forceActiveFocus()`
- `Delete` → remove if item selected
- `Return` → launch if item selected and installed

### 18. Smooth metadata load transition

**Problem:** When metadata finishes loading, content snaps in (detail pane, chips, icon). No animation.

**Fix:** Wrap detail pane content in `opacity: metadataLoaded ? 1 : 0` + `Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration } }`.

---

## Priority order for implementation

| # | Item | Effort | Impact |
|---|------|--------|--------|
| 1 | Launch button in detail pane | Low | High |
| 4 | Detail pane placeholder | Low | High |
| 15 | Settings close button | Trivial | High |
| 6 | Drag hint text | Low | High |
| 3 | Sort by date | Low | Medium |
| 2 | Desktop link toggle | Low | Medium |
| 9 | Relative paths in uninstall | Low | Medium |
| 11 | Collapse corpse warning | Low | Medium |
| 12 | Storage dialog section header | Trivial | Medium |
| 13 | Storage empty state | Trivial | Medium |
| 14 | Storage total size | Low | Medium |
| 16 | Settings label clarification | Trivial | Low |
| 7 | ManageWindow chips | Low | Low |
| 10 | Select All / Deselect All | Medium | Medium |
| 17 | Keyboard shortcuts | Medium | Medium |
| 5 | Homepage as readable link | Low | Low |
| 8 | About sheet → Dialog | Low | Low |
| 18 | Metadata load animation | Low | Low |
