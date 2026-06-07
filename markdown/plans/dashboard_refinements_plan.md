# Dashboard Layout Refinements and About Page Restyling

This plan details the implementation of layout refinements for the navigation sidebar, the online store catalog, and the About page.

---

## Proposed Changes

### [Component: Discover Page Layout and Sorting]

#### [MODIFY] [StorePage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/StorePage.qml)
- **Remove List Chips:** Remove the `Kirigami.Chip` inside the list item delegate that shows categories.
- **Under Search Bar Categories:**
  - Remove the "All" category button.
  - Make the category filter buttons checkable: clicking a checked button unchecks it and clears the `amStoreModel.categoryFilter` to `""` (showing all applications).
- **A-Z Sorting Button:**
  - Reorganize the top search area of the left pane into a `RowLayout`.
  - Add a sort button to the right of the search field. The button toggles `amStoreModel.sortAscending` and changes its icon between `view-sort-ascending` (A-Z) and `view-sort-descending` (Z-A).

#### [MODIFY] [amstoremodel.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.h)
- Add property `sortAscending`:
  ```cpp
  Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY sortAscendingChanged)
  ```
- Declare getters, setters, and signals for `sortAscending`.

#### [MODIFY] [amstoremodel.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.cpp)
- Implement `sortAscending` property methods.
- In `applyFilter()`, sort the filtered vector `m_apps` alphabetically by `displayName` in ascending or descending order depending on `sortAscending`.

---

### [Component: Native Navigation Sidebar with Collapse Support]

#### [MODIFY] [DashboardWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/DashboardWindow.qml)
- **Restructure Sidebar Layout:**
  - Split the sidebar column into a top `ListView` (Discover, Installed) and a bottom `ListView` (Settings, About).
  - Use `Kirigami.Theme` and rounded-corner highlights for list items, matching the native system list styling.
- **Collapsible Sidebar:**
  - Add property `property bool isSidebarCollapsed: false` at the root window scope.
  - In the Top Bar, add a hamburger or collapse action button (`sidebar-collapse` icon) that toggles `isSidebarCollapsed`.
  - When `isSidebarCollapsed` is true, change the sidebar width to a compact size (e.g., `Kirigami.Units.gridUnit * 2.5`), hide delegate text labels, and center list icons.
  - When expanded, restore normal width (`Kirigami.Units.gridUnit * 10`) and display both icons and text labels.

---

### [Component: FormCard Style for About Page]

#### [MODIFY] [AboutPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/AboutPage.qml)
- Rewrite the page layout to use `org.kde.kirigamiaddons.formcard` (similar to `SettingsPage.qml`).
- Replace custom `Rectangle` card layouts with FormCard equivalents:
  - **FormHeader:** "Application Info" -> **FormCard** enclosing app icon, name, version, and description.
  - **FormHeader:** "Project Links" -> **FormCard** containing delegates for License, Contribute, and Report an Issue.
  - **FormHeader:** "Libraries in Use" -> **FormCard** containing delegates for Qt, KDE Frameworks, and libappimage.
  - **FormHeader:** "Contributors" -> **FormCard** listing contributors with avatar initials and metadata delegates.

---

## Verification Plan

### Manual Verification
1. **Discover Page Filters:** Click category buttons in the Discover tab. Verify that clicking a button filters by category, and clicking it again clears the filter (showing all apps). Verify the "All" button is gone.
2. **Catalog List Items:** Verify list items in the Discover catalog no longer show category chips.
3. **Sort Integration:** Click the A-Z sorting button next to the search field. Verify that it reverses the sorting order (A-Z to Z-A) and updates the list dynamically.
4. **Collapsible Sidebar:** Click the sidebar toggle in the header. Verify the sidebar collapses to a thin bar displaying icons only, and expands back to display text and icons.
5. **Anchoring & Structure:** Verify "Settings" and "About" are anchored at the bottom of the navigation sidebar, while "Discover" and "Installed" remain at the top.
6. **About Page Design:** Navigate to "About". Verify it renders in the exact same `FormCard` card-box style as the Settings page.
