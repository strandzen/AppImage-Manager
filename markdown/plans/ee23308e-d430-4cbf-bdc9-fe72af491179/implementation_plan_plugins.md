# Plan: Break Qt Plugin Isolation to Enable System Themes

Even after explicitly setting the Qt Quick Controls style, the application remains visually isolated from the system theme. This is because `PyQt6` obtained via pip, and the resulting PyInstaller standalone binary, are entirely stripped of the host system's **Qt Platform Theme Plugins** (e.g., `KDEPlasmaPlatformTheme6.so`). Without these plugins, Qt ignores the system color scheme and defaults to `Fusion` with hardcoded fallback colors.

## Proposed Changes

### [Core]
#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Maintainer/main.py)
- Import `QCoreApplication` from `PyQt6.QtCore`.
- Before instantiating the `QGuiApplication`, call `QCoreApplication.addLibraryPath("/usr/lib/qt6/plugins")`. This explicitly instructs the Qt core to also scan the host system's plugin directory for platform integrations, alongside its bundled plugins. 

## Verification Plan

### Manual Verification
1. Rebuild the application binary.
2. The user will run the app to observe that the UI finally adheres perfectly to their active Plasma theme (colors, fonts, metrics).
