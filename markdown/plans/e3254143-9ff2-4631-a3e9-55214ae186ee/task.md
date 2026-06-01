# Zsync Updates & Daemon Implementation

- [x] Add `updateFrequency` and `customUpdateDays` properties to `AppSettings` (`appsettings.h/.cpp`).
- [x] Add the "Updates" UI section in `SettingsDialog.qml` (ComboBox & SpinBox).
- [x] Add `zsyncUrl`, `isUpdating`, and `updateProgress` to `AppImageListModel::Item` and role enums.
- [x] In `checkForUpdates()`, extract the actual `.zsync` URL from GitHub releases or strings.
- [x] Implement `downloadUpdate(int row)` in `AppImageListModel` using `QProcess` and `zsync`.
- [x] Handle `QProcess` stdout to extract progress percentage and emit `dataChanged`.
- [x] Handle `QProcess` finished to apply the update and emit `KNotification`.
- [x] Add `ProgressBar` and interaction to `DashboardWindow.qml` chip.
- [x] Create `UpdateDaemon` class (`updatedaemon.h/.cpp`) to check updates periodically.
- [x] Add `--daemon` flag to `main.cpp`.
- [x] Create `data/org.kde.appimagemanager.daemon.desktop`.
- [x] Update `CMakeLists.txt` and `src/CMakeLists.txt` to compile new files and install the autostart `.desktop`.
