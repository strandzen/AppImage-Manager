# Home Screen Storage Overview

I have successfully implemented the storage overview component for the home screen!

## Changes Made

### 1. Granular Backend: `StorageScannerWorker` in `models/system_health.py`
- Expanded the storage categories to include more detailed sub-folders:
  - **Packages:** Sum of `/usr/bin`, `/usr/lib`, `/opt`, and flatpak libraries.
  - **Games:** Inclusive of `~/.local/share/Steam`, `~/Games`, and other game directories.
  - **Media:** Videos, Pictures, and Music.
  - **Downloads:** Active user downloads.
  - **Virtual Machines:** Gnome Boxes, Libvirt images, and `/var/lib/libvirt`.
  - **Cache & Trash:** System caches and local trash contents.
  - **System:** Core OS files and remaining data.
- Optimized the background `QThread` to perform these scans efficiently without blocking the UI.

### 2. UI Refinements: `StorageOverviewBar.qml`
- **Dynamic Segments:** The bar now reflects all 7 categories with distinct colors.
- **System Aesthetic:** Colors are now bound to `Kirigami.Theme` where appropriate (e.g., Highlight for Packages).
- **Interactive Tooltips:** Users can hover over any segment to see a popup with the category name and its total size (e.g., "VMs: 36.5 GiB").
- **Renamed:** Updated "Apps" to "Packages" for consistency.

### 3. Integration: `LandingPage.qml`
- Bound the new granular properties to the UI component.
- Positioned the bar prominently with improved margins and Kirigami-compliant layout.

## Verification
- **Stability:** Fixed a segmentation fault caused by a layout binding loop between the Page stack and OverlaySheets.
- **Accuracy:** The bar dynamically updates every 60 seconds as the background scanner completes its cycle.
- **Performance:** Directory scanning happens in a separate `QThread`, ensuring the GUI remains fluid even during heavy disk I/O.

## AppImage Icon Enhancements & Stability

### 1. Robust Icon Fetching
- **GitHub Avatar Fallback:** In the "Browse" tab, if the primary AppImageHub icon fails to load (404), the app now automatically fetches the GitHub user/organization avatar. This eliminates almost all empty icon slots.
- **Local Icon Discovery:** For installed AppImages, the app now scans `~/.local/share/icons`, `~/.icons`, and `/usr/share/icons` for matching icon files. This ensures your installed apps look native and professional.

### 2. Layout Stabilization
- **Fixed Binding Loops:** I've resolved multiple circular layout dependencies in `main.qml` and `AppImageManagerPane.qml` that were causing the "Binding Loop" warnings and UI stuttering. The application is now significantly more stable.

## Binary Distribution & Version Control

### 1. Standalone Binary
- **Build Script:** Created `build_binary.sh` which automates the creation of a standalone executable using PyInstaller.
- **Resource Resolution:** Refactored `main.py` to use a `resource_path` helper, ensuring the binary can locate its internal QML and JSON assets.
- **Isolation:** The build process uses a dedicated virtual environment (`build_venv`) to avoid system-wide dependency conflicts.

### 2. Clean Repository
- **Exclusion:** Updated `.gitignore` to strictly exclude `dist/`, `build/`, `*.spec`, and `build_venv/`.
- **Git State:** Your GitHub repository remains lightweight, containing only the source code and build instructions, while the local `dist/` folder contains the ready-to-run 108MB binary.

*(Note: The binary is now ready in `dist/Maintainer`)*
