# AppImage Manager V1.1 Implementation Plan

This document outlines the technical implementation plan for the next iteration of the AppImage Manager, focusing on deeper Plasma integration, updating mechanics, and UI polish.

## Proposed Changes

### 1. Mandatory Plasma Search Visibility (KRunner Integration)
**Goal:** Ensure all `.AppImages` placed in the `Applications` folder are automatically visible in Plasma Search / KRunner by default.
- **Implementation:** 
  - Desktop shortcuts will become mandatory. 
  - Remove the "Create Desktop Shortcut" checkbox from the Manage GUI (`qml/ManageWindow.qml`).
  - Update `src/core/appimagemanager.cpp` so that `createDesktopLink()` is always invoked upon installation.

### 2. Window Decoration Titles
**Goal:** Clear and distinct window titles for the two main GUI entry points.
- **Implementation:**
  - **Dashboard:** Update `Kirigami.ApplicationWindow.title` in `qml/DashboardWindow.qml` to `"AppImage Dashboard"`.
  - **Manage GUI:** Update `Kirigami.ApplicationWindow.title` in `qml/ManageWindow.qml` to `"AppImage Manager"`.

### 3. Zsync Update Implementation
**Goal:** Implement robust delta-patching updates via `zsync`.
- **Implementation:**
  - Introduce `AppImageUpdater` (a new `QObject` C++ class).
  - Use `QProcess` to run the external `zsync` (or `zsync_curl`) binary asynchronously.
  - Parse `stdout` from the `zsync` process to calculate download progress.
  - Bind the update progress to the UI (updating the list item in `AppImageListModel` and replacing the placeholder Update button).
  - Fallback to standard full download (`KIO::CopyJob` or `QNetworkAccessManager`) if a `.zsync` file is not available.

### 4. Dynamic Dashboard Headline
**Goal:** Display the total amount of installed AppImages in the dashboard header.
- **Implementation:**
  - Update `qml/DashboardWindow.qml` to bind the page title to the list model count.
  - E.g., `title: i18n("Installed Applications (%1)", proxyModel.count)`.

### 5. Further UI Improvements
- General cleanup of empty states and padding within the Kirigami layouts.
- Smooth animations for state changes (e.g., when an update starts/finishes).

---

## Verification Plan

1. **Plasma Search:** Install a new AppImage via the UI, ensure no checkbox is presented, and immediately test if the app appears in KRunner.
2. **Titles:** Launch both modes (`appimagemanager` and `appimagemanager <path>`) and verify the OS window decorations display the correct titles.
3. **Dashboard Count:** Add/remove apps and observe the headline number updating dynamically.
4. **Zsync Updates:** Trigger an update on a supported AppImage, verify progress bars animate, and check that the file is successfully replaced.
