# Fix Theme Inheritance in PyInstaller Application

The goal is to ensure the PyInstaller-bundled application correctly inherits the system's "kde" theme (and dark mode). This requires bundling specific Qt plugins that are present on the system but missing from the Python environment's PyInstaller collection.

## User Review Required
> [!IMPORTANT]
> This change assumes the build is performed on a system with KDE libraries installed (`/usr/lib/qt6/plugins`), which seems to be the case. The resulting binary might be less portable to non-KDE systems, but will work correctly on the target KDE environment.

## Proposed Changes

### Build Configuration

#### [MODIFY] [ThemeSwitcher.spec](file:///home/herman/Documents/Project/Konsave/ThemeSwitcher.spec)
- Add `/usr/lib/qt6/plugins/platformthemes/KDEPlasmaPlatformTheme6.so` to the `binaries` list, targeting `plugins/platformthemes`.
- Add `/usr/lib/qt6/plugins/styles/breeze6.so` to the `binaries` list, targeting `plugins/styles`.

## Verification Plan

### Automated Tests
- Re-run the build command: `pyinstaller ThemeSwitcher.spec --clean`
- Verify the build completes successfully.
- Check the `dist` folder.
- Since I cannot run the GUI, I will inspect the build logs for any errors regarding these files.

### Manual Verification
- The user will need to run the generated executable `dist/ThemeSwitcher` and verify that the application now respects the system theme (dark mode).

## Proposed Changes (Layout Update)

### [MODIFY] [themeSwitcher](file:///home/herman/Documents/Project/Konsave/themeSwitcher)
- Create a `QHBoxLayout` (e.g., `btn_layout`).
- Add `self.apply_btn` to `btn_layout` with stretch factor or expanding size policy.
- Add `self.restart_btn` to `btn_layout`.
- Set `self.restart_btn` to have a smaller relative width (e.g., using stretch factors or explicit width).
- Add `btn_layout` to `right_layout`.

## Verification Plan (Layout Update)
- Run the script directly with python to verify the layout visually (user action).
