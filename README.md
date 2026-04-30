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

- **Dashboard**: Browse, search, and manage all installed AppImages in one window — with sort by name, size, or date added, alternating row colors, and per-row shortcut chip toggle.
- **Drag-and-Drop Installation**: Move AppImages to `~/Applications/` and make them executable with a simple gesture. Smooth snap-back animation returns the icon precisely to its origin if you cancel mid-drag.
- **Plasma Job Integration**: Install and remove operations show native Plasma progress bars in the system tray.
- **Dolphin Plugin**: Adds a "Manage AppImage" option to your right-click menu for instant access.
- **Desktop Integration**: Automatically creates application launcher shortcuts with native icon theme support.
- **Storage Analysis**: Inspect the AppImage file and any leftover config/cache files from a single dialog.
- **Clean Uninstallation**: Detects leftover `~/.config`, `~/.local/share`, and `~/.cache` directories and safely trashes them alongside the AppImage.
- **Safe by Design**: Moves files to the KDE Trash instead of permanent deletion, with clear confirmation dialogs and a one-click Restore notification action.
- **Configurable Notifications**: Toggle install/uninstall desktop notifications independently via Settings.
- **Performance Focused**: Metadata extraction and scanning run in the background for a responsive experience.

## Requirements

### Build Dependencies

- **C++20 Compiler** (GCC 12+ / Clang 15+)
- **CMake** (3.22+) + **Ninja**
- **Qt6** (6.6+ Core, Gui, Quick, Qml, Concurrent)
- **KDE Frameworks 6** (CoreAddons, I18n, KIO, IconThemes, Notifications, Crash, DBusAddons)
- **Kirigami 6**

### Optional

- `libappimage`: For faster in-process metadata extraction without FUSE.
- `libcanberra`: For an audio notification on installation completion.

## Installation

### Arch-based distros

```bash
sudo pacman -S base-devel cmake extra-cmake-modules ninja \
    qt6-base qt6-declarative \
    kcoreaddons ki18n kio kiconthemes knotifications kcrash kdbusaddons kirigami \
    libcanberra
```

### Build and Install

```bash
cmake --preset dev
cmake --build --preset dev
sudo cmake --install build/dev
```

To reload Dolphin without logging out:

```bash
kquitapp6 dolphin && dolphin &
```

## Usage

1. **Dashboard**: Run `appimagemanager --dashboard` or open it from the Dolphin right-click menu to see all installed AppImages.
2. **Install**: Right-click an `.AppImage` in Dolphin → **Manage AppImage** → drag the icon to the Applications folder.
3. **Launch**: Click the app icon in the manage window to run it immediately after installation.
4. **Remove**: Click the delete button in the dashboard, or **Remove** in the manage window, to clean up the AppImage and its associated files.

## License

GPL-2.0-or-later. See the [LICENSES](LICENSES/GPL-2.0-or-later.txt) folder for details.
