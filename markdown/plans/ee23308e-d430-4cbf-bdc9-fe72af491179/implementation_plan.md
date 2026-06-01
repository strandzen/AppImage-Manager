# Plan: Bundle Kirigami with PyInstaller

The application fails to run from the binary because the `org.kde.kirigami` QML module is not included in the bundle. Since the project uses a `pip`-installed `PyQt6`, it doesn't automatically see the system's Kirigami installation.

## Proposed Changes

### [Core]
#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Maintainer/main.py)
- In `main()`, after initializing `QQmlApplicationEngine`, add the application's base directory (using `resource_path(".")`) to the QML import paths. This allows the engine to find QML modules bundled at the root of the executable.

### [Build]
#### [MODIFY] [Maintainer.spec](file:///home/herman/Documents/Project/Maintainer/Maintainer.spec)
- Add `/usr/lib/qt6/qml/org/kde/kirigami` to the `datas` list, mapping it to `org/kde/kirigami` in the bundle.
- Ensure all other assets are correctly listed.

#### [MODIFY] [build_binary.sh](file:///home/herman/Documents/Project/Maintainer/build_binary.sh)
- Update the `pyinstaller` command to use the updated `Maintainer.spec` file instead of running from `main.py` directly, or update the command-line flags to include the Kirigami path.
- Preferred: Switch to `pyinstaller Maintainer.spec` to keep configuration centralized.

## Verification Plan

### Automated Tests
- None applicable for bundling issues.

### Manual Verification
1. Run `./build_binary.sh` to rebuild the binary.
2. Attempt to run the newly created binary: `dist/Maintainer`.
3. Verify that the application launches without the "module org.kde.kirigami is not installed" error.
4. Navigate through a few pages (e.g., Landing Page, Settings) to ensure Kirigami components (like `OverlaySheet` or `PageRow`) are rendering correctly.
