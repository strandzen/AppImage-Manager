# Zsync Delta Updates & Daemon Implementation Plan

This plan details how we will add zsync delta updates, progress notifications, a background daemon, and configurable settings.

## Proposed Changes

### 1. Update Settings & UI Configuration
We will add settings to control how often the update daemon checks for updates.

#### [MODIFY] src/core/appsettings.h & .cpp
- Add properties: `updateFrequency` (enum: Never, Daily, Weekly, Monthly, Custom) and `customUpdateDays` (int).
- Persist these values in the `KSharedConfig`.

#### [MODIFY] qml/SettingsDialog.qml
- Add a new "Updates" section.
- Add a `Kirigami.FormLayout` with a `ComboBox` for "Check for updates:" (Never, Daily, Weekly, Monthly, Custom).
- Add a `SpinBox` for "Custom interval (days)" that appears when "Custom" is selected.

---

### 2. Zsync Delta Updates
We will implement the logic to download updates using the system's `zsync` command, showing progress in the UI.

#### [MODIFY] src/gui/appimagelistmodel.h & .cpp
- **Identify Zsync URL**: In `checkForUpdates()`, when parsing GitHub releases, we will search the `assets` array for the file matching the `.zsync` wildcard and store its `browser_download_url` in a new `zsyncUrl` field in the `Item` struct.
- **Track State**: Add `bool isUpdating`, `int updateProgress` (0-100), and `QString zsyncUrl` to `Item`, with corresponding roles.
- **Download Method**: Add `Q_INVOKABLE void downloadUpdate(int row)`.
  - It will start a `QProcess` executing `zsync -i <current_file> -o <current_file>.new <zsync_url>`.
  - It will connect to `readyReadStandardOutput` to parse the percentage progress using a regular expression and emit `dataChanged` to update the UI.
  - On `finished` with exit code 0, it will replace the old `.AppImage` with the `.new` file, trigger a KNotification ("Update Completed"), and reload the model data.

#### [MODIFY] qml/DashboardWindow.qml
- If `model.isUpdating` is true, hide the version chip and display a `ProgressBar` showing `model.updateProgress`.
- Change the version chip interaction: if an update is available (and not currently updating), clicking the "Available" chip (or an adjacent download button) will trigger `listModel.downloadUpdate(index)`.

---

### 3. Update Daemon
A background daemon will run on login to check for updates periodically without launching the UI.

#### [NEW] src/core/updatedaemon.h & .cpp
- A lightweight class that checks the last update time against the `AppSettings` frequency.
- Uses `QDir` to list AppImages and parses their `X-AppImage-Update-Information` to perform network checks silently.
- If updates are found, it triggers a `KNotification` ("Update available for X. Click to manage."). The notification's default action will launch `appimagemanager`.

#### [MODIFY] src/main.cpp
- Add a `--daemon` command-line flag.
- If passed, instantiate `UpdateDaemon` and run `app.exec()` without opening the QML window.

#### [MODIFY] CMakeLists.txt & src/CMakeLists.txt
- Add `data/org.kde.appimagemanager.daemon.desktop` which executes `appimagemanager --daemon`.
- Install this `.desktop` file to `/etc/xdg/autostart` so it starts automatically on login.

## User Review Required
> [!IMPORTANT]
> - **Dependency Check**: This feature relies on the `zsync` command line utility being installed on the system (I verified it is available on your machine). If the user uninstalls it, updates will fail. I will add a check to disable the update button if `zsync` is missing.
> - **Daemon Implementation**: Using a `--daemon` flag with autostart is standard KDE practice. The daemon will use minimal memory as it won't load the Kirigami QML engine.
> Does this approach look good to you?
