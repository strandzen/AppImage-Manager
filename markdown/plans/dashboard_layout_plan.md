# Desktop-First Sidebar Layout and AM Store Integration

This plan outlines the restructuring of the AppImage Manager dashboard UI to support a desktop-first, multi-pane navigation layout. It implements a primary navigation sidebar and integrates the AM (AppMan) package repository.

---

## Goal Description
The current dashboard in [DashboardWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/DashboardWindow.qml) is a single-page view listing installed applications. We will reorganize this layout to match the requested 3-pane structure:
1. **Window Container (`Kirigami.ApplicationWindow`):** The primary host window.
2. **Top Header Bar:** A horizontal bar spanning the full window width, displaying the application title ("AppImage Manager") and active actions.
3. **Master Navigation Sidebar:** A left-hand list providing high-level modes: **Discover (Store)**, **Library (Installed)**, **Settings**, and **About**.
4. **Content Area:** A `StackLayout` on the right side switching views based on the active sidebar selection. The "Discover" and "Library" views will reuse the native master-detail (list + detail pane) aesthetic.

---

## User Review Required

> [!IMPORTANT]
> The current dialogs ([SettingsDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsDialog.qml) and [AboutDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/AboutDialog.qml)) will be converted to inline pages (`SettingsPage.qml` and `AboutPage.qml`) that render directly inside the content pane. The dialog files will be kept in the repository for now as backups, but will no longer be opened as standalone windows.

---

## Proposed Changes

We will restructure the QML layout by moving components into modular page files.

```
qml/
 ├── DashboardWindow.qml (Main container, Top Bar, Left Master List, StackLayout)
 ├── LibraryPage.qml     (NEW: Encapsulates current installed apps list + detail layout)
 ├── StorePage.qml       (NEW: AM store app list + detail layout)
 ├── SettingsPage.qml    (NEW: Extracted settings form card)
 └── AboutPage.qml       (NEW: Extracted about application info layout)
```

---

### [Component: Dashboard UI Reorganization]

#### [MODIFY] [DashboardWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/DashboardWindow.qml)
- Restructure the root content of `Kirigami.ApplicationWindow` to contain:
  - A top-anchored `Rectangle` or `ToolBar` acting as the **Top Bar (2)**.
  - A `RowLayout` filling the remaining area below the Top Bar.
  - A left-aligned `ListView` acting as the **Navigation Master List (3)** (listing: Discover, Library, Settings, About).
  - A `StackLayout` on the right acting as the **Content Pane (4)** showing:
    - Index 0: `StorePage` (Discover)
    - Index 1: `LibraryPage` (Installed)
    - Index 2: `SettingsPage` (Settings)
    - Index 3: `AboutPage` (About)
  - Connect actions like global shortcut `Ctrl+F` to forward focus to the active page's search field.

#### [NEW] [LibraryPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/LibraryPage.qml)
- Move the list, database watcher progress bar, detail pane, and background update action logic from the current `DashboardWindow.qml` (installed applications list and detail panel layout) into this page.
- Retains the exact HSL styling, chips, animations, and launcher behaviors.

#### [NEW] [StorePage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/StorePage.qml)
- Replicates the master-detail list/detail structure of `LibraryPage.qml` but binds to the online AM database.
- Left column shows categories and the global repository list (fetched from the cached SQLite database or online registry).
- Right column shows the detail panel, description card, and install progress indicators.

#### [NEW] [SettingsPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsPage.qml)
- Extracts the contents of [SettingsDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsDialog.qml) to run as an inline scrollable page inside the content area. Removes the standard window controls (Close/Restore defaults buttons move to page actions or bottom button rows).

#### [NEW] [AboutPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/AboutPage.qml)
- Extracts the contents of [AboutDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/AboutDialog.qml) to run as an inline scrollable page inside the content area.

---

## Verification Plan

### Manual Verification
1. **Window Layout Launch:** Build and launch the application (`./build/dev/bin/appimagemanager`). Verify the window opens with the horizontal top bar ("AppImage Manager") and the navigation master list on the left.
2. **Navigation Toggle:** Click through "Discover", "Installed", "Settings", and "About" in the left master list. Verify the right pane updates smoothly to show the corresponding view.
3. **Library Functionality:** Navigate to "Installed". Verify the list of applications, search field, update checker, and launch/remove functions work exactly as they did in the old single-page layout.
4. **Discover Layout:** Navigate to "Discover". Verify that the search list, card detailed view, and styling matches the layout style of the library page.
5. **Dialog Embedding:** Verify the Settings and About screens fit correctly within the right-hand panel without clipping.
