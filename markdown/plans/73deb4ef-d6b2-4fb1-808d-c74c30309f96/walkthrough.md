# Walkthrough: System Monitor Integration into Home Page

I have successfully moved the critical system monitoring metrics to the Landing (Home) Page and removed the standalone System Monitor page and all "processes" functionality.

## Changes Made

### UI Enhancements
- **[LandingPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/LandingPage.qml)**: 
    - Moved the application logo further up the page.
    - Embedded 2x2 metric panels for CPU, Memory, Download, and Upload under the logo.
    - Integrated the `StorageOverviewBar` and Swap usage progress bar below the metric panels.
- **[main.qml](file:///home/herman/Documents/Project/Maintainer/qml/main.qml)**: 
    - Updated navigation logic to hide the right-side Action Queue (task progress pane) specifically when the Landing (Home) Page is active.
    - Adjusted the center column to fill the available width on the Home page.
- **[SystemMonitorPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/SystemMonitorPage.qml)**: Deleted the redundant standalone monitor page and its tab-based interface.
- **[ui_left_list.json](file:///home/herman/Documents/Project/Maintainer/ui_left_list.json)**: Removed the "Monitor" sidebar entry.
- **[ui_strings.json](file:///home/herman/Documents/Project/Maintainer/ui_strings.json)**: Cleaned up redundant strings.

### Backend Simplification
- **[system_health.py](file:///home/herman/Documents/Project/Maintainer/models/system_health.py)**: 
    - Removed `ProcessModel` and all process-tracking logic (which was heavy on system resources).
    - Eliminated process-related timers and properties (`kill_process`, `toggleSort`, etc.).
    - Maintained only the core health metrics required for the Home page display.

## Verification Results
- **Home Page**: Metrics update in real-time below the centered logo.
- **Sidebar**: No "Monitor" icon/label remains.
- **Stability**: The application starts immediately and navigates correctly.
- **Resource Usage**: Expect slightly lower overhead due to the removal of continuous process list scanning.
