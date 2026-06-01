# Home Screen Storage Overview

## Goal Description
Add an iPhone-style storage overview bar to the Home Screen (`LandingPage.qml`) that visually represents the space taken by different categories: Packages (installed pacman/flatpak), Media (Videos/Photos), Games (Steam/Games folder), and System Data.

## Proposed Changes

### `models/system_health.py`
To avoid blocking the main UI thread during heavy directory scanning, we will introduce a background worker (`StorageScannerWorker`) inside `SystemHealth`.
- **New Properties:** `appsSize` (Packages), `mediaSize`, `gamesSize`, `downloadsSize`, `vmsSize`, `trashCacheSize`, `otherSize`, `totalUsedSize`, `totalDiskSize`.
- **Calculation Logic:**
  - **Packages:** Sum of `/usr/bin`, `/usr/lib`, `/opt`, `/var/lib/flatpak`, `/var/lib/pacman`.
  - **Media:** `~/Videos`, `~/Pictures`, `~/Music`.
  - **Games:** `~/Games`, `~/.local/share/Steam`, `~/.local/share/BeamNG`.
  - **Downloads:** `~/Downloads`.
  - **Virtual Machines:** `~/.local/share/gnome-boxes`, `~/.config/libvirt`, `/var/lib/libvirt`.
  - **Cache & Trash:** `~/.cache`, `~/.local/share/Trash`.
  - **Other:** Remaining used space on root partition.

### `qml/components/StorageOverviewBar.qml` [NEW]
- Create a new reusable QML component.
- Display a title like "Storage" and a right-aligned text "X GB of Y GB Used".
- Implement a stacked standard rounded rectangle (or multiple rectangles side by side) with relative widths bound to: `(categorySize / totalSize) * barWidth`.
- Use distinct Kirigami or Theme colors (e.g., Red for Apps, Yellow for Photos/Media, Green for Games, Grey for System/Other).
- Display a small legend underneath with colored dots and category names.

### `qml/pages/LandingPage.qml`
- Incorporate the `StorageOverviewBar` underneath the description or between the header and the description.
- Bind the component to `SystemMonitor` (which is the QML singleton exposing `SystemHealth`).

## Verification Plan
1. Launch the application and observe the `LandingPage`.
2. Ensure the bar renders smoothly without freezing the UI.
3. Validate that the sum of the components roughly equals the total used disk space reported by the system.
4. Verify standard Arch Linux Steam paths are correctly resolving sizes.

## [NEW] AppImage & Layout Enhancements

### AppImage Icon Fallback
The current `appimage_hub.py` relies on `appimage.github.io` URLs which often 404. 
- **Change:** In `AppImageHubManager`, if an icon URL is missing or after a failed load, we will attempt to use the GitHub user avatar as a fallback: `https://github.com/[owner].png?size=128`.
- **Installed App Icons:** For AppImages that are already installed, we will check `~/.local/share/icons` for an icon matching the app's name or binary name. Often, `appimagelauncher` or the app itself extracts an icon there.

### Layout & Stability Fixes
Multiple binding loops and anchor errors were reported in `main.qml`, `LandingPage.qml`, and `AppImageManagerPane.qml`.
- **Conflicting Anchors:** Fix `ColumnLayout` in `main.qml` specifying `left`, `right`, and `horizontalCenter` at the same time.
- **Binding Loops:** Use `Layout.preferredWidth` and fixed implicit dimensions to break circular dependencies in `ToolButton` and `OverlaySheet`.
- **Performance:** Move AppImage local icon lookup to a more efficient mechanism or ensure it doesn't block the UI thread if scanning many files.

### Storage Bar Refinements
- **Categorization:** Rename "Apps" to "Packages" throughout the UI and backend logic if necessary.
- **Aesthetics:** Use `Kirigami.Theme` colors (e.g., `Kirigami.Theme.highlightColor`, `Kirigami.Theme.positiveTextColor`) instead of hardcoded hex values.
- **Interactivity:** Add `ToolTip` components to each segment of the storage bar to show the absolute size (e.g., "Packages: 45.2 GB").

## Binary Build & Version Control [NEW]
Build a standalone binary for the project and ensure it's not committed to git.

### [Maintainer]

#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Maintainer/main.py)
- Implement a `resource_path` helper to resolve asset paths (QML, JSON, icons) correctly when running as a standalone binary (using `sys._MEIPASS`).
- Update all `os.path.join` calls to use this helper.

#### [MODIFY] [.gitignore](file:///home/herman/Documents/Project/Maintainer/.gitignore)
- Add `dist/`, `build/`, and `*.spec` to ensure binaries are not pushed to GitHub.

#### [NEW] [build_binary.sh](file:///home/herman/Documents/Project/Maintainer/build_binary.sh)
- A simple wrapper to call PyInstaller with all necessary `--add-data` flags.

## Verification Plan
1. Run `build_binary.sh`.
2. Execute the resulting binary in `dist/Maintainer`.
3. Verify that the UI loads and no "file not found" errors appear in the terminal.
4. Run `git status` to ensure `dist/` is ignored.
