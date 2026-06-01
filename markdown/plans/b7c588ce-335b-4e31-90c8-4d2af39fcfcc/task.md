# Tasks — Native KDE Discover Integration (Local AppStream Export)

- `[x]` Implement `useDiscover` setting in `AppSettings` (`src/core/appsettings.h/.cpp`)
- `[x]` Add "Manage installed applications via KDE Discover" checkbox in `SettingsDialog.qml` (`qml/SettingsDialog.qml`)
- `[x]` Configure CLI entry point (`src/main.cpp`) to launch `plasma-discover --page installed` when `useDiscover` is active and no arguments are passed
- `[x]` Implement AppStream XML Export on installation (`src/core/appimagemanager.cpp`)
- `[x]` Implement retroactive sync helper to write XML for pre-existing AppImages
- `[x]` Implement AppStream XML cleanup on uninstallation (`src/core/appimagemanager.cpp`)
- `[x]` Verify compilation, run unit tests, and perform manual integration verification with KDE Discover
