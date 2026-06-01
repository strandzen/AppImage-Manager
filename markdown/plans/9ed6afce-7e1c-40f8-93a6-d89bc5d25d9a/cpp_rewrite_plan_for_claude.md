# KDE Plasma C++ Native Rewrite Plan

**Objective:** Rewrite the existing Python/PySide6 `AppImageManager` into a native C++20 application using Qt6 and KDE Frameworks 6 (KF6). The resulting codebase must be architecturally sound and compliant with KDE's upstream standards (e.g., suitable for `kde-cli-tools` or KDE Gear).

**Target Audience:** This document serves as a strict implementation prompt/specification for Claude Code (or any AI coding assistant).

---

## 1. Tech Stack & Dependencies

The new project must use CMake and standard KDE development tools.

*   **Build System:** CMake 3.16+, Extra CMake Modules (ECM).
*   **Language:** C++20.
*   **GUI:** Qt6 (QML), KF6 Kirigami.
*   **Libraries:** 
    *   `Qt6::Core`, `Qt6::Gui`, `Qt6::Qml`, `Qt6::Quick`
    *   `KF6::I18n` (Translations)
    *   `KF6::CoreAddons` (KAboutData)
    *   `KF6::KIOCore`, `KF6::KIOFileWidgets` (Native file ops, Trash)
    *   `KF6::Service` (KBuildSycoca, KDesktopFile)
    *   `KF6::IconThemes` (Icon fallback logic)
    *   `libappimage` or `libsquashfs` (For direct memory-mapped metadata extraction, bypassing FUSE mounting).

## 2. Project Structure

```text
appimagemanager/
├── CMakeLists.txt
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── backend.h
│   ├── backend.cpp
│   ├── corpsemodel.h
│   ├── corpsemodel.cpp
│   └── appimageparser.h / .cpp
├── qml/
│   ├── ManageWindow.qml
│   └── UninstallWindow.qml
└── org.kde.appimagemanager.desktop
```

## 3. Phase-by-Phase Implementation Instructions

### Phase 1: Build System & Entry Point (CMake & `main.cpp`)
1.  Initialize `CMakeLists.txt` using `find_package(ECM REQUIRED NO_MODULE)`.
2.  Link all required KF6 and Qt6 components.
3.  In `main.cpp`, instantiate `QGuiApplication`, initialize `KAboutData::setApplicationData`, and configure `KLocalizedContext` to support standard `i18nc()` translations in QML.
4.  Load `ManageWindow.qml` via `QQmlApplicationEngine`.

### Phase 2: Metadata Parser (`AppImageParser.cpp`)
*   **Task:** Replace Python's `subprocess.run(["squashfuse"])`.
*   **Implementation:** Use `libsquashfs` or `libappimage` API to read the SquashFS offset of the AppImage.
*   **Extraction:** Extract the `.desktop` file and the root `.DirIcon` (or `usr/share/icons/...`) directly into memory (e.g., `QByteArray`).
*   **Caching:** Store extracted metadata in a C++ struct. Do not extract twice.

### Phase 3: The C++ Backend (`AppImageManagerBackend.cpp`)
*   **Task:** Port `gui.py` and `core.py` logic to a `QObject` derived class exposed to QML via `qmlRegisterType` or `QML_ELEMENT`.
*   **Async execution:** Use `QThreadPool::globalInstance()` and `QRunnable` or `QtConcurrent::run` for metadata extraction and finding corpses. Emit Qt signals (e.g., `metadataLoaded()`) upon completion.
*   **Trash Integration:** Replace `shutil.rmtree` with `KIO::trash(QUrl::fromLocalFile(path), KIO::HideProgressInfo)`. Use a `KIO::Job` and connect to its `result()` signal to know when the deletion is done.
*   **Install Integration:** Replace `kioclient move` subprocess with `KIO::moveAs()`. 

### Phase 4: Desktop & Icon Integration (`KDesktopFile` & `KIconTheme`)
*   **Desktop Files:** Replace Python string concatenation for `.desktop` generation with `KDesktopFile`. Use `KConfigGroup` to securely write `Exec`, `Icon`, and `Name` keys. This guarantees XDG escaping.
*   **Exec Sanitization:** Instead of Regex, split arguments safely. `KDesktopFile` handles escaping automatically when saving.
*   **Icon Fallback:** Use `KIconTheme::current()->hasIcon(app_id)` to check if the system theme provides the icon (e.g., `krita`). If true, set `Icon=krita` in the `.desktop` file. If false, extract the `QByteArray` icon, save it to `~/.local/share/icons/hicolor/256x256/apps/`, and use that path.

### Phase 5: The Corpses Model (`CorpseModel.cpp`)
*   **Task:** Replace the static QML `ListModel` for uninstalling.
*   **Implementation:** Create a custom `QAbstractListModel` subclass (`CorpseModel`).
*   **Roles:** Define roles like `Qt::UserRole + 1` for `FilePathRole`, `FileSizeRole`, and `IsCheckedRole`.
*   **Benefit:** Native C++ models handle huge lists (e.g., thousands of cache files) without lagging the QML interface.

## 4. Specific Pitfalls for Claude to Avoid
1.  **Do not use `QProcess` for KDE tasks:** If there is a KF6 library for it (`KIO`, `KService`), use the library. `QProcess` is only acceptable as a last resort.
2.  **Thread Safety:** Remember that `KIO::Job` must be started on the main GUI thread, even if `QtConcurrent::run` is used for disk scanning. Use `QMetaObject::invokeMethod` to pass results from worker threads back to the main thread before starting a KIO job.
3.  **AppImage Type 1:** Add a 16-byte magic number check in the `AppImageParser`. If the AppImage lacks the `AI\x02` SquashFS magic bytes (Type 1), either reject it or fall back gracefully. Do not let `libsquashfs` segfault.
