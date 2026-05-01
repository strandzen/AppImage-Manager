# AppImage Manager

A lightweight AppImage manager for KDE Plasma 6. Built with AI assistance.

[![KDE Plasma 6](https://img.shields.io/badge/KDE_Plasma-6-1d99f3?logo=kde&logoColor=white)](https://kde.org/plasma-desktop/)
[![Qt](https://img.shields.io/badge/Qt-6.6%2B-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/License-GPL--2.0--or--later-blue)](LICENSES/GPL-2.0-or-later.txt)
[![Linux](https://img.shields.io/badge/Platform-Linux-FCC624?logo=linux&logoColor=black)](https://www.kernel.org/)

![Drag-and-drop installation](assets/AppImage.gif)
![Dolphin right-click menu](assets/context_menu.png)
![Security disclaimer](assets/preinstall.png)
![Manage window](assets/installed.png)

## Features

- **Dashboard** — browse, search, and sort all installed AppImages by name, size, or date
- **Dolphin plugin** — right-click any `.AppImage` to open the manage window
- **Drag-and-drop install** — drag the icon to your Applications folder to install
- **Desktop shortcuts** — creates launcher entries with icons in your app menu
- **Clean uninstall** — finds leftover config/cache dirs and moves everything to the KDE Trash
- **Storage view** — see the AppImage file size and related directories at a glance
- **Background update checks** — daemon scans for newer versions via GitHub Releases and sends a desktop notification
- **Plasma integration** — install/remove show native Plasma progress bars; notifications are configurable

## Requirements

- CMake 3.22+, Ninja, C++20 compiler (GCC 12+ / Clang 15+)
- Qt 6.6+: Core, Gui, Quick, Qml, Concurrent, Network
- KDE Frameworks 6: CoreAddons, I18n, KIO, IconThemes, Notifications, Crash, DBusAddons, Kirigami

### Optional

- `libappimage` — faster in-process metadata extraction (no FUSE required)

## Build & Install

Install dependencies (Arch):

```bash
sudo pacman -S base-devel cmake extra-cmake-modules ninja \
    qt6-base qt6-declarative qt6-networkauth \
    kcoreaddons ki18n kio kiconthemes knotifications kcrash kdbusaddons kirigami
```

Build and install:

```bash
cmake --preset dev
cmake --build --preset dev
sudo cmake --install build/dev
```

Reload the Dolphin plugin without logging out:

```bash
kquitapp6 dolphin && dolphin &
```

## Usage

| Command | What it does |
| ------- | ------------ |
| `appimagemanager` | Open the dashboard |
| `appimagemanager /path/to/app.AppImage` | Open the manage window for a specific file |
| `appimagemanager --daemon` | Run the background update checker |

The daemon is also registered as a KDE autostart entry — it starts automatically on login once installed.

**Right-click in Dolphin:** select **Manage AppImage** on any `.AppImage` file.

## License

GPL-2.0-or-later — see [LICENSES/GPL-2.0-or-later.txt](LICENSES/GPL-2.0-or-later.txt).
