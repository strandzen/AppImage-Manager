# AppImage Manager

A lightweight AppImage manager for KDE Plasma 6, designed for simplicity and seamless desktop integration.

<p align="center">
  <img src="assets/AppImage.gif" alt="Drag-and-drop installation demo" width="45%" />
  <img src="assets/context_menu.png" alt="Dolphin context menu" width="45%" />
</p>
<p align="center">
  <img src="assets/preinstall.png" alt="Security disclaimer before installation" width="90%" />
</p>
<p align="center">
  <img src="assets/installed.png" alt="Main interface showing installed AppImages" width="90%" />
</p>

---
> **Disclaimer:** This project utilizes AI-assisted development (Claude/Gemini) for refactoring and feature implementation.

## Features

- **Drag-and-Drop Installation**: Move AppImages to `~/Applications/` and make them executable with a simple gesture.
- **Dolphin Plugin**: Adds a "Manage AppImage" option to your right-click menu for instant access.
- **Desktop Integration**: Automatically creates application launcher shortcuts with native icon theme support.
- **Clean Uninstallation**: Safely removes leftover configuration and cache files when deleting an app.
- **Safe by Design**: Moves files to the KDE Trash instead of permanent deletion, with clear confirmation dialogs.
- **Performance Focused**: Metadata extraction and scanning run in the background for a responsive experience.

## Requirements

### Build Dependencies

- **C++20 Compiler** (GCC 12+ / Clang 15+)
- **CMake** (3.22+)
- **Qt6** (6.6+ Core, Gui, Quick, Qml, Concurrent)
- **KDE Frameworks 6** (CoreAddons, I18n, KIO, IconThemes, Notifications, Crash, DBusAddons)
- **Kirigami 6**

### Optional

- `libappimage`: For faster metadata extraction without FUSE.
- `libcanberra`: For completion sound notifications.

## Installation

### Quick Start (Arch Linux)

```bash
sudo pacman -S base-devel cmake extra-cmake-modules qt6-base qt6-declarative kcoreaddons ki18n kio kiconthemes knotifications kcrash kdbusaddons kirigami libcanberra
```

### Build and Install

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
sudo cmake --install build
```

*Note: Restart Dolphin or log out/in after installation to activate the context menu plugin.*

## Usage

1. **Install**: Right-click an `.AppImage` in Dolphin → **Manage AppImage** → Drag the icon to the Applications folder.
2. **Launch**: Click the app icon in the manager window to run it immediately.
3. **Remove**: Click **Remove** in the manager to clean up the AppImage and its associated configuration files.

## License

GPL-2.0-or-later. See the [LICENSES](LICENSES/GPL-2.0-or-later.txt) folder for details.
