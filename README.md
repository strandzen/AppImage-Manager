# AppImage Manager

<p align="center">
  <img src="assets/AppImage.gif" width="45%" />
  <img src="assets/context_menu.png" width="45%" />
</p>

A lightweight, native AppImage manager for KDE Plasma.

AppImage Manager integrates directly into your KDE desktop environment to handle AppImage files efficiently. It replaces the manual process of moving files, making them executable, and creating menu shortcuts with a clean, macOS-style installation window.

## Features

- **macOS-Style Installation**: Drag and drop the AppImage into the Applications folder directly from the popup window.
- **Context Menu Plugin**: Adds a "Manage AppImage" option to the Dolphin right-click context menu.
- **Smart Desktop Integration**: Automatically uses your system's icon theme (e.g., Papirus, YAMIS) for a native look. If none is found, it falls back to the AppImage's internal icon. Preserves original `Exec` arguments safely.
- **Clean Uninstallation to Trash**: Asynchronously scans `~/.config`, `~/.cache`, and `~/.local/share` for leftover files ("corpses"). Items are safely moved to the KDE Trash instead of being permanently deleted.
- **Lightning Fast**: Metadata extraction and file scanning run asynchronously in the background, keeping the UI snappy and responsive at all times.

## Installation

To compile and install AppImage Manager, you need a C++ compiler and CMake:

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

This will install the standalone binary, the QML files, and the KIO Service Menu so that the "Manage AppImage" option appears in Dolphin.

### Uninstallation

To remove the application from your system, simply run the `uninstall` target from your build directory:

```bash
cd build
sudo make uninstall
```

## Usage

**Installing an AppImage:**
1. Right-click any `.AppImage` file and select "Manage AppImage".
2. Drag the application icon to the folder icon in the center of the window. The file will be moved to `~/Applications/` and made executable.
3. Check the "Create Shortcut" box to add it to your system application launcher.

**Uninstalling an AppImage:**
1. Right-click an installed AppImage and select "Manage AppImage".
2. Click "Remove".
3. A window will list the AppImage file along with any related configuration or cache directories found on your system. Select the items you want to delete and confirm.

## Requirements

AppImage Manager is a native C++ application built on Qt6 and KDE Frameworks 6.

### Build Dependencies
You must have the following development packages installed to compile the project:
- **C++20 Compiler** (GCC or Clang)
- **CMake** & **Extra CMake Modules (ECM)**
- **Qt6**: `qt6-base`, `qt6-declarative`
- **KDE Frameworks 6**: `kirigami`, `ki18n`, `kcoreaddons`, `kio`, `kservice`, `kiconthemes`

#### Quick Install (Build Dependencies)

**Ubuntu/Debian/KDE Neon:**
```bash
sudo apt install build-essential cmake extra-cmake-modules qt6-base-dev qt6-declarative-dev libkf6kirigami-dev libkf6i18n-dev libkf6coreaddons-dev libkf6kio-dev libkf6service-dev libkf6iconthemes-dev
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake extra-cmake-modules qt6-base qt6-declarative kirigami ki18n kcoreaddons kio kservice kiconthemes
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel kf6-kirigami-devel kf6-ki18n-devel kf6-kcoreaddons-devel kf6-kio-devel kf6-kservice-devel kf6-kiconthemes-devel
```

### Runtime Dependencies
- **System Icons**: Custom icon themes, like YAMIS, Hatter or Papirus, are fully supported and prioritized for desktop shortcuts. 
- **squashfuse / fusermount3**: Used as a fallback for mounting AppImages if `libappimage` is not available at compile time.

> **Note on AppImage Formats:** AppImage Manager is heavily optimized for **Type 2 AppImages** (which use `squashfs`). Type 1 AppImages (older ISO9660 format) are supported but metadata extraction may be limited.

## Roadmap

Future planned features for AppImage Manager:
- **Delta Updates**: Implementation of `zsync` / `AppImageUpdate` for automatic delta updates of installed AppImages.
- **Orphan Cleanup Daemon**: A background service utilizing `inotify` to automatically remove `.desktop` shortcuts when an AppImage is manually deleted from the `~/Applications` folder.
