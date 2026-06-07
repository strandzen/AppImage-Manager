# Walkthrough - Desktop-First Layout and AM Store UI Integration

We have restructured the AppImage Manager dashboard interface to transition from a single-page app layout into a clean, multi-page, desktop-first navigation layout matching your mockup guidelines, and integrated the real C++ AM package repository backend.

## Changes Made

### 1. Navigation Restructuring
- **Modified [DashboardWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/DashboardWindow.qml):**
  - Integrated the **Top Bar (2)** at the top of the window displaying "AppImage Manager" and a quick "Install from File" button.
  - Implemented the **Master List Navigation Panel (3)** on the left with options for **Discover**, **Installed**, **Settings**, and **About**.
  - Structured a **StackLayout Content Pane (4)** on the right to toggle between views based on the active selection.
  - Added global shortcut focus handling so pressing `Ctrl+F` focuses the active page's search field.

### 2. Page Extraction & Component Modularization
- **Created [LibraryPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/LibraryPage.qml):** Extracts the installed apps list, details layout, HSL outline styles, chips, launchers, and drag-and-drop installer triggers. Exposes its `searchField` property.
- **Created [SettingsPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsPage.qml):** Converts [SettingsDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsDialog.qml) into a scrollable, embedded page for changing application configurations.
- **Created [AboutPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/AboutPage.qml):** Converts [AboutDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/AboutDialog.qml) into a scrollable, embedded page for viewing version details and contributors.

### 3. AM Repository Integration (Real Discover Functionality)
- **Created [amstoremodel.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.h) and [amstoremodel.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.cpp):**
  - Uses `QNetworkAccessManager` to download the live database of x86_64 AppImages (`https://raw.githubusercontent.com/ivan-hc/AM/main/programs/x86_64-appimages`).
  - Parsed using string tokenizers, mapping description keywords into high-level categories (Development, Games, Internet, Graphics, Office, Multimedia, System).
  - Implements rapid C++ searching and category filtering on the data vectors.
  - Spawns background `QProcess` to run `appman -i <pkg>` or `am -i <pkg>` for native, rootless installations. If `appman` isn't installed locally, it downloads it to `~/.local/bin/appman` automatically.
  - Pipes terminal output line-by-line via signals.
- **Modified [dashboardwindow.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/dashboardwindow.h) and [dashboardwindow.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/dashboardwindow.cpp):**
  - Instantiated `AMStoreModel` and registered it as the `amStoreModel` QML context property.
- **Created [StorePage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/StorePage.qml):**
  - Replaces mock data and filters directly on `amStoreModel` properties.
  - Added an **Installation Output** console drawer that slides open and logs process stdout/stderr dynamically during app installations.

### 4. Build Configuration
- **Modified [CMakeLists.txt](file:///home/herman/Documents/Project/AppImageManager/src/CMakeLists.txt):**
  - Registered the new page files (`LibraryPage.qml`, `StorePage.qml`, `SettingsPage.qml`, and `AboutPage.qml`) as raw resources.
  - Added the C++ sources (`gui/amstoremodel.cpp` and `gui/amstoremodel.h`) to the `appimagemanager_qml` compilation target.

---

## Verification Results

### Build Successful
The project compiled successfully without any errors or warnings:
```bash
cmake --build --preset dev
```

### Unit Tests Passed
All 10 unit tests executed and passed:
```
Test project /home/herman/Documents/Project/AppImageManager/build/dev
      Start  1: appstreamtest
 1/10 Test  #1: appstreamtest ..............................   Passed    0.01 sec
      Start  2: appimagemanager-tst_appimageinfo
 2/10 Test  #2: appimagemanager-tst_appimageinfo ...........   Passed    0.00 sec
      Start  3: appimagemanager-tst_appimagereader
 3/10 Test  #3: appimagemanager-tst_appimagereader .........   Passed    0.04 sec
      Start  4: appimagemanager-tst_appimagecache
 4/10 Test  #4: appimagemanager-tst_appimagecache ..........   Passed    0.04 sec
      Start  5: appimagemanager-tst_appimagelistmodel
 5/10 Test  #5: appimagemanager-tst_appimagelistmodel ......   Passed    0.42 sec
      Start  6: appimagemanager-tst_appimageiconprovider
 6/10 Test  #6: appimagemanager-tst_appimageiconprovider ...   Passed    0.43 sec
      Start  7: appimagemanager-tst_appsettings
 7/10 Test  #7: appsettings ................................   Passed    0.06 sec
      Start  8: appimagemanager-tst_updatedaemon
 8/10 Test  #8: appimagemanager-tst_updatedaemon ...........   Passed    0.26 sec
      Start  9: appimagemanager-tst_appimagedbusadaptor
 9/10 Test  #9: appimagemanager-tst_appimagedbusadaptor ....   Passed    0.04 sec
      Start 10: appimagemanager-tst_qml
10/10 Test #10: appimagemanager-tst_qml ....................   Passed    1.05 sec

100% tests passed, 0 tests failed out of 10
```
