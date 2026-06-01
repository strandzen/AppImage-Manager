# Plan - Fix Font Reversion and Clean Up Debug Messages

The objective is to ensure that clearing the font settings accurately reverts the UI to the system default font and to remove unnecessary debug print statements.

## Proposed Changes

### Models

#### [MODIFY] [settings_manager.py](file:///home/herman/Documents/Project/Maintainer/models/settings_manager.py)
- Add `defaultFontFamily` and `defaultFontSize` properties to store and expose the system's original font settings.

---

### Backend

#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Maintainer/main.py)
- Remove the debug print statement: `🚀 MAINTAINER APP STARTING - DEBUG VERSION 2.0.1 (Markers Fix)`.
- Initialize the new `defaultFontFamily` and `defaultFontSize` properties in `SettingsManager` using the captured `system_default_font`.

---

### UI

#### [MODIFY] [main.qml](file:///home/herman/Documents/Project/Maintainer/qml/main.qml)
- Update font bindings to use `SettingsManager.defaultFontFamily` and `SettingsManager.defaultFontSize` as fallbacks when settings are empty/zero.

#### [MODIFY] [SidebarItem.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/SidebarItem.qml)
- Sync with `main.qml` to use the explicit system defaults for fallbacks.

#### [MODIFY] [SystemMonitorPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/SystemMonitorPage.qml)
- Increase `Layout.rightMargin` for `listHeader` and the process list `ItemDelegate`'s `RowLayout` to provide more breathing room between the "kill" icon and the vertical scrollbar.

### Phase 5: UI Refinement - Conditional Separators

#### [MODIFY] [SystemMonitorPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/SystemMonitorPage.qml)
- Set `visible: !SettingsManager.alternatingRowColors` for the `Kirigami.Separator` inside the process list delegate.

#### [MODIFY] [PackageManagerPane.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/PackageManagerPane.qml)
- Set `visible: !SettingsManager.alternatingRowColors` for the `Kirigami.Separator` inside the package list delegate.

#### [MODIFY] [CorpseCleanerPane.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/CorpseCleanerPane.qml)
- Set `visible: !SettingsManager.alternatingRowColors` for the `Kirigami.Separator` inside the results list delegate.

#### [MODIFY] [TaskProgressPane.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/TaskProgressPane.qml)
- Set `visible: !SettingsManager.alternatingRowColors` for the `Kirigami.Separator` inside the "Ready" and "Running" list delegates.

#### [MODIFY] [AppImageManagerPane.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/AppImageManagerPane.qml)
- Set `visible: !SettingsManager.alternatingRowColors` for the `Kirigami.Separator` inside the list delegates.

### Phase 6: Fix Process List Clipping

#### [MODIFY] [SystemMonitorPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/SystemMonitorPage.qml)
- Add a vertical `Kirigami.Separator` to the `ListView` to separate content from the scrollbar.
- Add `rightPadding: Kirigami.Units.gridUnit` to the `ListView`.
- Update the `ItemDelegate` width to `appList.width - appList.rightPadding` to prevent background/separator overlap with the scrollbar.
- Align `listHeader` margins with the `ListView` padding.

## GitHub Push Plan

> [!IMPORTANT]
> **No changes will be pushed to GitHub without explicit user permission.**

- Stage all changes (Settings refactor, Font settings, System Monitor layout, QML stabilization, Separator refinements).
- Commit with clear, descriptive summaries once work is verified.
- **WAIT** for user approval before running `git push`.

## Verification Plan

### Manual Verification
1. **Remove Debug Print:** Run the app and confirm the "🚀 MAINTAINER APP STARTING" message no longer appears in the terminal.
2. **Font Customization & Reversion:** Verify font settings apply instantly and revert correctly when cleared.
3. **Process List Spacing:** 
    - Open System Monitor -> Processes.
    - Confirm there is more space between the skull icons and the right edge/scrollbar.
    - Confirm headers align correctly with the column data.
