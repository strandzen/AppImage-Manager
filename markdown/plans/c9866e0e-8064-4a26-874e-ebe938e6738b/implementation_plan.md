# AppImage Build and Cleanup for Konsave

This plan outlines the steps to clean the codebase and package the Python/PyQt6 application as an AppImage locally.

## Proposed Changes

### [Cleanup]
- Remove unneeded files and directories: `app.yml`, `build/`, `dist/`, `build_env/`, `.github/`.
- [NEW] Create a `build_appimage.sh` script for local builds.

### [Python Code [DONE]]
- Paths fixed in `themeSwitcher`.

### [Build Configuration]
#### [MODIFY] [ThemeSwitcher.spec](file:///home/herman/Documents/Project/Konsave/ThemeSwitcher.spec)
- Include `Previews/` and `Presets/` directories in the PyInstaller bundle.

### [Local Build Script]
#### [NEW] [build_appimage.sh](file:///home/herman/Documents/Project/Konsave/build_appimage.sh)
- Script to:
    - Build PyInstaller executable.
    - Download `linuxdeploy` if missing.
    - Package as AppImage.

## Verification Plan

### Automated Tests
- None available for this project.

### Manual Verification
- Run `./build_appimage.sh`.
- Test the resulting `.AppImage` file.
