# Project Cleanup Walkthrough

The project has been cleaned up and the AppImage/GitHub work has been discarded as requested.

## Changes Made

### Cleanup
- Removed the following unnecessary files and directories:
    - `.git`, `.github`, `.gitignore`
    - `ThemeSwitcher.desktop`, `ThemeSwitcher.spec`, `ThemeSwitcher.png`
    - `build/`, `dist/`, `build_env/`, `app.yml`, `build_appimage.sh`

### Code Restoration
- Cleaned the [themeSwitcher](file:///home/herman/Documents/Project/Konsave/themeSwitcher) script:
    - Restored all necessary imports (sys, os, subprocess, PyQt6).
    - Simplified path handling to use a `BASE_DIR` relative to the script's location, making it more robust for local execution.

### PyInstaller Build
- Created a [ThemeSwitcher.spec](file:///home/herman/Documents/Project/Konsave/ThemeSwitcher.spec) file.
- Built the application into a standalone executable: [dist/ThemeSwitcher](file:///home/herman/Documents/Project/Konsave/dist/ThemeSwitcher).
- The executable includes all necessary PyQt6 libraries and the `Previews/` folder.

## Verification
- Verified the build output in `dist/`.
- The executable `dist/ThemeSwitcher` (~89MB) is ready for use.
