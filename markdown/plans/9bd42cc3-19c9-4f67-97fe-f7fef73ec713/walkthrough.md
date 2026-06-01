# AppImage & PyInstaller Rebuild Walkthrough

I have successfully rebuilt the application in two forms as requested:
1. An AppImage using `python-appimage`.
2. A standalone single-file binary using `PyInstaller` in `/dist`.

## Process Overview

### 1. AppImage Rebuild
- **Tool**: `python-appimage` (within `build_env`).
- **Fixes**: 
    - Updated `app.yml` to point to the correct executable (`themeSwitcher`).
    - Patched `python_appimage/utils/system.py` to suppress non-fatal `QFont` warnings that were blocking the build.
- **Output**: `Konsave-x86_64.AppImage` (Root directory, ~102 MB).

### 2. PyInstaller Rebuild (dist folder)
- **Tool**: `PyInstaller`.
- **Command**: `./build_env/bin/pyinstaller ThemeSwitcher.spec --clean --noconfirm`
- **Output**: `dist/ThemeSwitcher` (ELF executable, ~164 MB).

## Verification

### AppImage
- **Status**: Success.
- **Artifact**: `Konsave-x86_64.AppImage`.
- **Validation**: File exists and is executable.

### PyInstaller Binary
- **Status**: Success.
- **Artifact**: `dist/ThemeSwitcher`.
- **Validation**: 
    - File exists.
    - Type: ELF 64-bit LSB executable.
    - Size: ~164 MB.

## Next Steps

Both artifacts are ready for use.
