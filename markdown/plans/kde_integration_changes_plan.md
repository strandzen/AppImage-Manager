# Plan — AppImage Manager KDE Plasma 6.X Integration & Robustness

This plan outlines the surgical changes required to align **AppImage Manager** with KDE system standards, improve background efficiency, resolve security risks in updates and installations, and clean up QML module dependencies.

## User Review Required

> [!WARNING]
> **Removal of Installer Bootstrapping:** We will remove the code that automatically downloads and executes the third-party `appman` shell script via raw GitHub curls. Users will be notified that `am` or `appman` must be installed on their system.
> **Removal of QtWidgets Dependency:** We will migrate the folder picker from C++ `QFileDialog` to QML `FolderDialog`. This allows removing `Qt6::Widgets` from the QML shared library target.

---

## Proposed Changes

### Component 1: Security & Integrity in Downloader

#### [MODIFY] [appimageupdate.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/appimageupdate.cpp)
* Extract file hash calculation into a shared helper: `static QString calculateFileSha1(const QString &filePath)`.
* In `startFullHttpDownload`:
  * Parse the `SHA-1` header from the downloaded `.zsync` metadata file.
  * Once the binary download finishes, calculate its SHA-1 hash off-thread using `QtConcurrent::run` with the helper.
  * Compare the local hash with the parsed remote hash. If they do not match, delete the downloaded `.new` file, emit `downloadFinished(filePath, false)`, and show a warning notification.

---

### Component 2: Network Rate-Limiting & Concurrency Control

#### [MODIFY] [updatedaemon.h](file:///home/herman/Documents/Project/AppImageManager/src/core/updatedaemon.h)
* Add a dedicated, low-concurrency thread pool member `QThreadPool m_readerPool` (bounded to 2–4 threads).
* Add a check queue member `QQueue<AppImageInfo> m_updateQueue`.
* Declare `void checkNextUpdate()` helper.

#### [MODIFY] [updatedaemon.cpp](file:///home/herman/Documents/Project/AppImageManager/src/core/updatedaemon.cpp)
* In `checkUpdates()`:
  * Instead of `QtConcurrent::mapped` on the global thread pool, loop through the files list and run `AppImageReader::read` via `QtConcurrent::run(&m_readerPool, ...)`.
  * Track pending read counts with an atomic counter. Once all reads finish and metadata is loaded, filter those with `gh-releases-zsync|` into `m_updateQueue`.
  * Reset `m_updateCount = 0` and call `checkNextUpdate()`.
* Implement `checkNextUpdate()`:
  * Dequeue the next `AppImageInfo` and run `GitHubReleaseChecker`.
  * In the checker's finished lambda, recursively call `checkNextUpdate()` to serialize the API network requests.
  * If the queue is empty, call `updateTrayStatus()`.

#### [MODIFY] [appimageupdate.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/appimageupdate.h)
* Add a check queue member `QQueue<CheckItem> m_checkQueue` for serializing the UI update check.

#### [MODIFY] [appimageupdate.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/appimageupdate.cpp)
* Refactor `checkForUpdates(items)` to push check items into `m_checkQueue` and process them sequentially via a recursive helper `checkNextItem()`, preventing parallel rate-limiting issues on the GitHub API during UI catalog checks.

---

### Component 3: Desktop Integration & Placeholders

#### [MODIFY] [appimagemanager.cpp](file:///home/herman/Documents/Project/AppImageManager/src/core/appimagemanager.cpp)
* In `createDesktopLink()`:
  * Inspect the AppImage's internal desktop metadata or original name to identify if it supports document arguments (e.g. if the original exec had `%u`/`%U`/`%f`/`%F`).
  * If it is a generic utility, append `%U` to the `Exec` line so file associations and dragging files onto launcher items work correctly.
* In `iconFilePath()`:
  * Store the extracted icons in the standard XDG path:
    * SVGs: `~/.local/share/icons/hicolor/scalable/apps/appimage_<fileKey>.svg`
    * PNGs: `~/.local/share/icons/hicolor/256x256/apps/appimage_<fileKey>.png`
  * In `createDesktopLink`, write the simple icon name `appimage_<fileKey>` instead of the absolute path into the desktop file's `Icon` field.

---

### Component 4: Discover Store & Native Installer

#### [MODIFY] [amstoremodel.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.h)
* Declare a download state tracker and fields to support in-app HTTP downloads for Hub-only packages.

#### [MODIFY] [amstoremodel.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.cpp)
* In `installApp(packageName)`:
  * Check if the package has an AM installer script.
  * **Remove the script bootstrap curl process.** If the system is missing `am` / `appman` and the package requires it, log a warning and emit installation failure.
  * If the package has no AM script but has a direct `downloadUrl`, transition to a native in-app download:
    * Fetch the AppImage via `QNetworkAccessManager` streaming to `AppSettings::applicationsPath()`.
    * Emit progress status logs.
    * Once downloaded, set executable permissions, run `AppImageReader::read()` on the downloaded file, and register it via `createDesktopLink()`.

---

### Component 5: QtWidgets Removal & QML Dialog Migration

#### [MODIFY] [appsettings.h](file:///home/herman/Documents/Project/AppImageManager/src/core/appsettings.h)
* Remove `openFolderPicker()`.
* Expose `Q_INVOKABLE void setApplicationsPathFromUrl(const QUrl &url)`.

#### [MODIFY] [appsettings.cpp](file:///home/herman/Documents/Project/AppImageManager/src/core/appsettings.cpp)
* Remove `openFolderPicker()` and the `#include <QFileDialog>` header.
* Implement `setApplicationsPathFromUrl(url)` converting `QUrl` to a local filesystem path.

#### [MODIFY] [CMakeLists.txt](file:///home/herman/Documents/Project/AppImageManager/src/CMakeLists.txt)
* Remove `Qt6::Widgets` from the `appimagemanager_qml` target link libraries list.
* Add `Qt6::Widgets` explicitly to the standalone binary target `appimagemanager_bin`.

#### [MODIFY] [SettingsPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsPage.qml) and [SettingsDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsDialog.qml)
* Declare a standard `FolderDialog` component from `QtQuick.Dialogs`.
* When clicked, call `folderDialog.open()` and on accepted call `AppSettings.setApplicationsPathFromUrl(folderDialog.selectedFolder)`.


---

### Component 6: Documentation and Packaging Updates

#### [MODIFY] [PKGBUILD](file:///home/herman/Documents/Project/AppImageManager/packaging/aur/PKGBUILD)
* Add `am-git` under `optdepends` (or `depends`) and specify that it is required for installing applications from the AM Database script repository.

#### [MODIFY] [README.md](file:///home/herman/Documents/Project/AppImageManager/README.md)
* Document `am` / `appman` under the requirements/dependencies sections as an optional package dependency for Discover page AM Database functionality.

---


## Verification Plan

### Automated Tests
* Run standard test suites:
  ```bash
  cmake --build --preset dev
  ctest --test-dir build/dev --output-on-failure
  ```

### Manual Verification
* **Update Verification:** Trigger a manual update check on the library and ensure update checks are executed sequentially. Check fallback update path and verify it fails if the file hash is tampered with.
* **MIME/File Association:** Associate an AppImage with a file type in Dolphin and verify that double-clicking the file launches the AppImage with the file path correctly passed (due to `%U` integration).
* **Discover Installation:** Install a Hub-only package in the Discover catalog and verify it downloads natively inside the app, creates the icon in the hicolor theme directory, and populates the installed app list.
* **Settings Dialog:** Open the folder picker in settings and check that it uses the native QML folder selection window.
