# Migrate Settings to KConfigXT

This plan outlines the migration of `AppSettings` to use KDE's standard `KConfigXT` system instead of manually interacting with `KSharedConfig`.

## User Review Required
No breaking changes. The QML API remains exactly the same, ensuring that the UI does not need to be modified.

## Proposed Changes

### Core Library & QML Module

#### [NEW] [appimagemanager.kcfg](file:///home/herman/Documents/Project/AppImageManager/src/core/appimagemanager.kcfg)
Create the XML schema that defines the configuration entries, their types, and default values.
- `applicationsPath` (String, default: `~/Applications`)
- `showDisclaimer`, `showNotifications` (Bool)
- `updateFrequency`, `customUpdateDays`, `manageIconSize` (Int)
- `watchDownloads`, `showInstallBox`, `accentBorders` (Bool)

#### [NEW] [appimagemanager.kcfgc](file:///home/herman/Documents/Project/AppImageManager/src/core/appimagemanager.kcfgc)
Create the configuration file for the `kconfig_compiler` to generate the `AppImageManagerSettings` class. It will define `File=appimagemanagerrc`, `ClassName=AppImageManagerSettings`, and `Singleton=true`.

#### [MODIFY] [CMakeLists.txt](file:///home/home/herman/Documents/Project/AppImageManager/src/CMakeLists.txt)
- Add `kconfig_add_kcfg_files(KCFG_SRCS core/appimagemanager.kcfgc)`
- Append `${KCFG_SRCS}` to the `SOURCES` list of the `qt_add_qml_module` target.

#### [MODIFY] [appsettings.h](file:///home/herman/Documents/Project/AppImageManager/src/core/appsettings.h)
- Remove `KSharedConfig` include and member variable.
- Keep the `Q_PROPERTY` and `Q_INVOKABLE` definitions untouched, so QML keeps working identically.

#### [MODIFY] [appsettings.cpp](file:///home/herman/Documents/Project/AppImageManager/src/core/appsettings.cpp)
- Remove the raw `m_config->group(...).readEntry(...)` lines.
- Implement getters/setters by delegating to `AppImageManagerSettings::self()->[property]()` and `AppImageManagerSettings::self()->set[Property](...)`.
- Preserve the custom validation in `setApplicationsPath` (emitting `applicationsPathError`) before calling the `AppImageManagerSettings` setter.
- When `set[Property]` changes a value, emit the existing change signals and call `AppImageManagerSettings::self()->save()` to ensure it is written to disk.

## Verification Plan

### Automated Tests
- Run `ctest --test-dir build/dev --output-on-failure` to ensure that core tests still pass and we didn't break basic application loading.

### Manual Verification
- Compile the application (`cmake --build --preset dev`).
- Open the settings dialog in the dashboard and modify various settings (e.g., toggle "Show Security Disclaimer" or change the applications path).
- Verify that `~/.config/appimagemanagerrc` updates correctly.
- Verify that closing and reopening the application preserves the settings correctly.
