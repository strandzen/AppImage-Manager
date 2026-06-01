# Implementation Plan - Settings Dialog UX & Visual Redesign

This plan outlines the visual redesign and functional enhancements for the **Settings Dialog** in AppImage Manager to match KDE Plasma 6 Kirigami best practices.

## Proposed Changes

### 1. C++ Settings Controller (`src/core/`)

#### [MODIFY] [appsettings.h](file:///home/herman/Documents/Project/AppImageManager/src/core/appsettings.h)
- Declare `Q_INVOKABLE void resetToDefaults();` to enable restoring settings safely from QML.

#### [MODIFY] [appsettings.cpp](file:///home/herman/Documents/Project/AppImageManager/src/core/appsettings.cpp)
- Implement `resetToDefaults()`, resetting all fields (`applicationsPath`, `showDisclaimer`, `showNotifications`, `updateFrequency`, `customUpdateDays`, `manageIconSize`, `watchDownloads`, `showInstallBox`, `accentBorders`) to their factory values and calling `m_config->sync()`.

---

### 2. User Interface Components (`qml/`)

#### [MODIFY] [SettingsDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsDialog.qml)
- **Discover-Style Card Panels**: Replace the flat `Kirigami.FormLayout` with four individual rounded cards (`Rectangle` elements with background, padding, and borders).
- **Accent Borders Synergy**: Bind card borders in SettingsDialog to `AppSettings.accentBorders ? Kirigami.Theme.focusColor : Kirigami.Theme.hoverColor`.
- **Descriptive Subtitles**: Add detailed, low-opacity subtitles under every checkbox to guide the user.
- **Dolphin Shortcut**: Add an "Open folder in file manager" icon button next to the folder path selector using `Qt.openUrlExternally("file://" + AppSettings.applicationsPath)`.
- **Restore Defaults**: Place a "Restore Defaults" button with a `document-revert` icon at the bottom of the layout, calling `AppSettings.resetToDefaults()`.

---

## Verification Plan

### Automated Verification
- Compile the C++ additions:
  ```bash
  cmake --build --preset dev
  ```
- Run CTest suite to verify regression safety.

### Manual Verification
- Open Settings, verify the beautiful card layout and checkboxes with detailed subtitles.
- Click the "Open folder" button and confirm Dolphin opens the Applications folder.
- Modify multiple settings (turn off accent borders, change icon size, change update interval), click "Restore Defaults", and verify all values revert cleanly.
- Turn off accent borders and verify the cards in the Settings page itself transition instantly from blue to gray outlines.
