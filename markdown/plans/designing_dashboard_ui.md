# Master-Detail Dashboard UI Implementation Plan

> [!IMPORTANT]
> **For Claude**: You must adhere strictly to the architecture, philosophy, and conventions detailed in both `CLAUDE.md` and `GEMINI.md`. 
> - **Philosophy**: Simplicity and efficiency. No unnecessary abstractions. 
> - **Stack**: C++20, Qt6, KDE Frameworks 6, Kirigami (Plasma 6 era). 
> - All new components must use `Kirigami.Theme` appropriately and match the aesthetic principles outlined in the documentation.

## Goal Description
Overhaul the `DashboardWindow.qml` to implement a new Master-Detail split layout, accurately reflecting the approved wireframes and incorporating recent UX feedback. 

## Proposed Changes

### [MODIFY] [DashboardWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/DashboardWindow.qml)

**1. Header Reorganization**
- **Remove** the current `actions:` array.
- **Add** a custom `header:` item using a `RowLayout`.
- **Left side**: `Kirigami.Heading` reading "Installed Applications".
- **Right side**: A `RowLayout` containing `Controls.ToolButton`s for "Check all for updates" (icon: `view-refresh`) and "Settings" (icon: `configure`). Note: Ensure the action triggers remain connected to `listModel.checkForUpdates()` and `settingsDialog.open()`.

**2. Layout Restructuring (Populated State)**
- Replace the main `ListView` and headers with a `QtQuick.Controls.SplitView`.
- **Left Pane (Master List)**:
  - This column contains the Search Bar and the List.
  - **Search Bar**: Move the `Kirigami.SearchField` from the header to be the *very first entry* visually above the list in this left pane.
  - **List View**: Simplified `delegate`. The delegate should be a rounded `Rectangle` containing only the `appName` (`model.displayName`).
  - **Sizing**: Implement dynamic sizing for the left pane. Its `SplitView.preferredWidth` should automatically adjust to fit the length of the longest entry name (e.g., using `ListView`'s `contentItem.childrenRect.width` or calculating the maximum `implicitWidth` of the delegates). This is the default opening state.
  - **Selection State**: The delegate background must clearly indicate when `listView.currentIndex === index` using `Kirigami.Theme.highlightColor`. Clicking a delegate sets it as the current index.
- **Right Pane (Detail View)**:
  - Visible only when `listView.count > 0` and `listView.currentIndex >= 0`.
  - Bound to `proxyModel` data for the currently selected index.
  - **Constraints**: Set a strict `SplitView.minimumWidth` (e.g., `Kirigami.Units.gridUnit * 15`) to ensure the user cannot resize the left pane to the point where the info-panel becomes unusable. The right pane should have `SplitView.fillWidth: true`.
  - **Icon**: Large `Kirigami.Icon` (`source: modelData.iconSource`) centered at the top.
  - **Name**: `Kirigami.Heading` centered below the icon.
  - **Metadata Chips**: A `RowLayout` of `Kirigami.Chip` elements:
    - Version: `text: "Version: " + modelData.version`
    - Size: `text: "Size on disk: " + modelData.formattedSize`
  - **Action Buttons**: A `RowLayout` at the bottom containing:
    - `Controls.Button` for "Update". **Important**: Set `enabled: false` for now as requested by the user. The update function will be implemented later.
    - `Controls.Button` for "Remove". Clicking this triggers `uninstallDialog.open()` with the `filePath` of the selected item.

**3. Empty State Layout**
- Visible only when `proxyModel.count === 0 && !listModel.scanning`.
- Create a central drop area container that mimics the visual style of `ManageWindow.qml` exactly:
  - Use a `Rectangle` with `color: Kirigami.Theme.alternateBackgroundColor`.
  - `radius: Kirigami.Units.smallSpacing * 2`.
  - `border.color: Kirigami.Theme.focusColor` and `border.width: 1` (expanding to `2` on drag enter).
- **Contents**: Unlike `ManageWindow`, do **not** include the draggable AppImage icon or the folder icon.
- **Placeholder**: Center a single placeholder `Kirigami.Icon` (e.g., `application-x-executable`).
- **Text**: Below the icon, center a `Label` or `Kirigami.Heading` with the text: "Drag AppImage here to install".

**4. Code Cleanup**
- Remove all traces of the old multi-column layout (Name, Version, Size, Shortcut buttons, Remove column).
- Remove the old `Kirigami.PlaceholderMessage`.
- Clean up obsolete `readonly property int col...Width` variables at the top of the file.

## Verification Plan

### Automated/Code Verification
- `cmake --build --preset dev` must succeed with zero QML warnings.

### Manual Verification
1. **Empty State**: Launch the app with no AppImages. Verify the container matches the `ManageWindow.qml` aesthetic, containing only a placeholder icon and the text "Drag AppImage here to install".
2. **Populated State**: Launch with AppImages.
3. **Split View Dynamics**: Verify the left pane opens at a width that perfectly fits the longest app name. Resize the split handle to the right and confirm it stops at the minimum width threshold of the right pane.
4. **Search Position**: Verify the search bar is located at the top of the left pane, not in the global window header.
5. **Selection**: Click items in the left list. Verify the right pane updates accurately and instantly.
6. **Actions**: 
   - Verify the "Update" button is visible but completely disabled.
   - Click "Remove" -> verify the correct uninstall dialog appears inline.
