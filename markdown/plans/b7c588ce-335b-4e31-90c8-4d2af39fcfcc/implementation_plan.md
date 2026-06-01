# Implementation Plan: Native KDE Discover Integration (Revised)

This document outlines the revised plan to integrate AppImage Manager with the KDE Discover software center, utilizing the standard AppStream XML export method.

---

## Part 1: Layman Version

### Goal
The goal is to allow KDE users to manage their currently installed AppImage applications directly from the standard KDE Discover application (the Installed tab in the Software Center) instead of using our standalone dashboard. A new setting will be added to AppImage Manager to prioritize KDE Discover over our dashboard.

### Why We Revised the Plan
Our system analysis showed that KDE Discover does not provide public software developer files (C++ headers) on standard Linux distributions. This means compiling a custom C++ plugin is not possible without duplicating and compiling Discover's entire internal source code, which would break every time Discover receives an update.

Instead, we will use a much more robust, standard-compliant method: **Local AppStream XML Export**. By exporting standard metadata files when installing an AppImage, Discover will automatically scan, discover, and display our AppImages natively in the "Installed" list without needing a custom plugin.

### What the Setting Does
A new checkbox is added to our Settings dialog: "Manage installed applications via KDE Discover" (this is already fully coded!).

When this setting is enabled:
* Running the AppImage Manager with no arguments (such as clicking its icon in your application launcher) will open KDE Discover directly to its Installed page instead of opening our custom Dashboard window (this is already fully coded!).
* All first-install behaviors are preserved: right-clicking an uninstalled AppImage in Dolphin and selecting "Manage AppImage" will still open our custom drag-and-drop window so you can easily install the app.
* Download notifications will still notify you of new files and open the custom install window on click so you can perform the first-install drag-and-drop.
* Once installed, instead of using our custom dashboard to view and manage your files, you manage them inside Discover.

### What the User Will See in Discover
* Discover's Installed tab will list all currently installed AppImages, displaying their icons, file sizes, versions, and descriptions.
* Clicking "Remove" inside Discover will delete the AppImage binary, its desktop launcher, and its icon, moving them directly to the system Trash.
* Background updates will continue to be handled by our optimized background update daemon, which notifies you via standard system tray updates.

---

## Part 2: Pure Technical Version

### Target Architecture
KDE Discover utilizes the standard Freedesktop AppStream specification to display installed software. Discover actively monitors the local directory `~/.local/share/metainfo/` for XML metadata files. 

If a `.desktop` file matches an AppStream XML file in these metadata paths, Discover registers the application as installed under its standard "Installed" view, enabling native properties visualization and uninstallation.

```
+-------------------------------------------------------------+
|                        KDE Discover                         |
|  - Scans ~/.local/share/metainfo/ for XML metadata          |
|  - Displays matched apps in the native Installed list       |
+-------------------------------------------------------------+
                               ^
                               | (Reads XML & Desktop files)
                               |
+-------------------------------------------------------------+
|                  Local Filesystem Storage                   |
|  - ~/.local/share/applications/appimage_<id>.desktop        |
|  - ~/.local/share/metainfo/appimage_<id>.appdata.xml        |
+-------------------------------------------------------------+
                               ^
                               | (Writes files during install)
                               |
+-------------------------------------------------------------+
|                    appimagemanager_core                     |
|  - Extracts AppStream XML from AppImage binary on install   |
|  - Writes metadata files directly to user directories       |
+-------------------------------------------------------------+
```

### Technical Implementations

#### 1. AppSettings & CLI Redirect (Fully Implemented)
* Implemented the `useDiscover` boolean setting in `AppSettings`.
* Configured `src/main.cpp` to intercept empty-argument runs and launch `plasma-discover --page installed` via `QProcess::startDetached` when `useDiscover` is active, exiting immediately with 0.

#### 2. AppStream XML Export on Installation (src/core/appimagemanager.cpp)
* **Goal**: Write the extracted AppStream XML metadata to the user's local metainfo folder during installation.
* **Implementation Details**:
  * In `AppImageManager::createDesktopLink(const QString &appImagePath, const AppImageInfo &info)`:
    * Construct the local metainfo directory path: `~/.local/share/metainfo/`.
    * Ensure the directory exists using `QDir().mkpath()`.
    * Generate the XML filename: `~/.local/share/metainfo/appimage_<slug>.appdata.xml`.
    * Reconstruct a standard AppStream XML file wrapping the parsed `info.description`, `info.homepage`, and `info.developerName` under standard tags, or write out the raw parsed XML data.
    * Write the XML file to the destination path.
  * In `AppImageManager::removeDesktopLink(const QString &appImagePath, const AppImageInfo &info)`:
    * Remove the corresponding XML file from `~/.local/share/metainfo/` on uninstallation to clean up Discover's registry.

---

## 📋 Step-by-Step Implementation Phase

1. **Phase 1: AppStream Metadata Export (1 Day)**
   * Implement XML generation and file writing inside `AppImageManager::createDesktopLink` and removal inside `AppImageManager::removeDesktopLink`.
2. **Phase 2: Retroactive Sync Utility (1 Day)**
   * Write a small helper that scans currently installed AppImages and retroactively generates AppStream XML files for them, ensuring already-installed AppImages show up in Discover immediately on first activation.
3. **Phase 3: Verification (1 Day)**
   * Verify compilation, install AppImages, launch Discover, and validate that installed apps display natively and can be uninstalled.
