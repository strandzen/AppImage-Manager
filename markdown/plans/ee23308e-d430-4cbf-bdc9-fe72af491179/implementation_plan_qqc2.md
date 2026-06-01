# Plan: Force Qt Quick Controls 2 Style (Breeze)

The application colors still look generic because the Qt Quick engine is falling back to the `Basic` or `Fusion` style. This happens because the exact Qt Quick Controls style (`org.kde.desktop`) is not natively bundled with the `pip` version of `PyQt6` used for building, and Qt needs explicit instructions to use it and find where the system stores the style plugins.

## Proposed Changes

### [Core]
#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Maintainer/main.py)
- Import `QQuickStyle` from `PyQt6.QtQuickControls2`.
- Set the style explicitly before the application event loop: `QQuickStyle.setStyle("org.kde.desktop")`.
- Additionally, ensure the application can locate the system-installed QML plugins by appending `/usr/lib/qt6/qml` to the `QML2_IMPORT_PATH` environment variable, or by adding it directly via `engine.addImportPath("/usr/lib/qt6/qml")`.
- Set `QT_QPA_PLATFORMTHEME="kde"` via environment variables before app creation just to be certain.

### [Build]
#### [MODIFY] [Maintainer.spec](file:///home/herman/Documents/Project/Maintainer/Maintainer.spec)
- Ensure the `org.kde.desktop` style plugin and basic QQC2 styles from KDE might also need to be explicitly added to the bundle if we want pure portability. But since we established it's a Linux system app and already rely on system Kirigami, bridging the system path (`/usr/lib/qt6/qml`) in `main.py` is more reliable.

## Verification Plan
1. I will implement these changes and rebuild the binary.
2. The user will run the binary to verify if the UI finally snaps into the Plasma system theme (Breeze).
