# AppImageManager Review & Improvement Plan

This plan aims to refine AppImageManager by adopting Senior KDE Plasma developer best practices. We will focus on architectural simplicity, C++ efficiency, and a more fluid, "premium" Kirigami UI.

## User Review Required

> [!IMPORTANT]
> The `ManageWindow` will be changed from a fixed-size window to a more flexible one that follows Kirigami standards. This might slightly alter the "compact" feel but will improve accessibility and consistency.

## Proposed Changes

### 1. C++ Backend Refinement (Simplicity & Efficiency)

#### [MODIFY] [appimagelistmodel.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/appimagelistmodel.h) / [appimagelistmodel.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/appimagelistmodel.cpp)
* Add `DisplayNameRole` and `FormattedSizeRole`.
* Move size formatting logic from QML to C++ using `KFormat`.
* Simplify `IconSourceRole` logic.
* Ensure `AppImageManager` usage is efficient (avoiding redundant object creation).

#### [MODIFY] [appimagebackend.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/appimagebackend.h) / [appimagebackend.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/appimagebackend.cpp)
* Add `displayName` and `formattedSize` properties.
* Centralize name cleaning logic.

### 2. QML Fluidity & Premium Feel

#### [MODIFY] [ManageWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/ManageWindow.qml)
* Replace hardcoded margins/spacing with `Kirigami.Units`.
* Remove fixed window constraints; use `implicitWidth`/`implicitHeight`.
* Enhance drag-and-drop animations with smoother transitions and better visual feedback.
* Refactor the "About" sheet into a cleaner, more standard Kirigami layout.
* Simplify the security disclaimer layout.

#### [MODIFY] [DashboardWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/DashboardWindow.qml)
* Use the new `displayName` and `formattedSize` roles.
* Simplify the `ItemDelegate` structure.
* Remove manual background color logic in favor of Kirigami's standard states.
* Improve the layout of "Chips" for size and version.

### 3. Polish & Maintenance

#### [MODIFY] [UninstallWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/UninstallWindow.qml)
* Apply similar spacing and layout improvements.

---

## Verification Plan

### Automated Tests
* Verify that the application still builds correctly (`cmake --build build`).
* Check for any QML warnings in the terminal during execution.

### Manual Verification
* Test the drag-and-drop installation flow in `ManageWindow`.
* Verify that "Shortcut" toggles work correctly in both windows.
* Check that the Dashboard correctly reflects changes (install/uninstall/shortcut).
* Verify that the "About" sheet looks and feels premium.
