# Plan: Improve System Theme Adhesion

The application currently defaults to a generic QML theme which doesn't perfectly match the system's Breeze (KDE) palette. This is because system palette inheritance and Kirigami's theme inheritance aren't explicitly enabled.

## Proposed Changes

### [Core]
#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Maintainer/main.py)
- In `main()`, before creating `QGuiApplication`, set `Qt.ApplicationAttribute.AA_UseHighDpiPixmaps` and `Qt.ApplicationAttribute.AA_EnableHighDpiScaling` (good practice for modern Linux).
- **Key Change**: Ensure the application inherits the system palette by setting `QGuiApplication.setDesktopSettingsAware(True)`.

#### [MODIFY] [main.qml](file:///home/herman/Documents/Project/Maintainer/qml/main.qml)
- In the `ApplicationWindow`, set `Kirigami.Theme.inherit: true`.
- Update the background color logic to be more resilient. Currently, it uses `Qt.darker(Kirigami.Theme.backgroundColor, UIColors.theme.window_darker_multiplier)`. This works, but we should make sure `Kirigami.Theme.colorSet` is set to `Kirigami.Theme.Window` to ensure it picks up the correct base colors.

## Verification Plan

### Automated Tests
- None.

### Manual Verification
1. Run the application (non-bundled) to check for immediate visual changes: `python main.py`.
2. Observe if the colors match the system's active KDE theme (e.g., Breeze Dark or Breeze Light).
3. Rebuild the binary (`./build_binary.sh`) and run it to ensure the bundled version also correctly inherits the theme.
