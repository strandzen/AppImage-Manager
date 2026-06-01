# Maintainer Project Migration & Implementation Plan

This plan addresses the transition of the current QML UI to a C++20 backend, implementing the system maintenance logic for CachyOS/Arch, and refining the UI according to the Project Vision.

## Goal Description
Evolve the existing PyQt6/QML prototype into a fully functional system maintenance tool while keeping Python. This includes creating modular Python classes for system tasks (Clean, Maintain), a backend execution engine, and dynamic system health monitoring. The UI will also be refined (e.g., expandable taskbar) per the Project Vision.

## Proposed Changes

### 1. Landing Page & System Health (`models/system_health.py`)
- **SSD Health**: Integrate `smartctl` (via `pkexec` or parsing without root if possible, or using a background polling thread that requests privileges once) to get actual remaining life percentage and temperature.
- **Storage Overview**: While `shutil.disk_usage` works for `/`, we will implement `QStorageInfo` in PyQt6 to get detailed capacity/usage for both Root (`/`) and Home (`/home`).
- **RAM**: Keep `/proc/meminfo` parser.

### 2. Clean Module (Space Reclamation)
Add these to `tasks_config.json` and implement size calculation logic (pre-execution dry runs):
- **Orphaned Packages** (Recommended): `pacman -Rns $(pacman -Qtdq)`.
- **Package Cache** (Recommended): Use settings variable to keep top 2-3 versions.
- **Systemd Journald Vacuuming**: Use settings variable for time threshold (e.g., 7 days).
- **Unused Flatpak Runtimes**: `flatpak uninstall --unused`.
- **User Cache**: Selective clearing of `~/.cache/` for resource-heavy apps (requires a custom python task rather than a simple command, or a complex shell script).

### 3. Maintain Module (System Integrity)
Add these to `tasks_config.json`:
- **Configuration Audit (Ghost Configs)**: Replaces "Broken Config Symlinks". Scans `~/.config` and `~/.local/share` for orphaned directories by cross-referencing against installed packages (`pacman -Qq`) and flatpaks. 
  - *Interactive Feature*: When `calculate_size()` is run, it generates a list of removable folders. The UI will show a "Review" button allowing the user to selectively check/uncheck these folders before confirming tasks.
- **Systemd Service Health**: `systemctl --failed` with option to restart (might need interaction or just report).
- **Systemd Service Health**: `systemctl --failed` with option to restart (might need interaction or just report).
- **Pacman Lock-file Repair**: Safely check and `rm /var/lib/pacman/db.lck` if no pacman process is running.
- **Disk Health Check (S.M.A.R.T.)**: Active deep-dive diagnostic reporting task.

### 4. Settings Module
- Create `qml/pages/SettingsPage.qml` accessible from the sidebar footer.
- Implement `models/settings_manager.py` (using `QSettings`) to store variables like:
    - Number of package caches to keep (default 3)
    - System log age threshold (default 7 days)
    - Ghost Config Whitelist (list of folders to always ignore)
- Bind these settings into the `TaskRegistry` or individual `CommandTask` instances.

### 5. Task Engine Enhancements (`models/task_model.py` & `tasks/base_task.py`)
- **Size calculation**: Add a `calculate_size()` method to `BaseTask` to perform a dry-run or `du -sh` before execution, exposing a `reclaimedSpace` property to the QML model.
- **Reviewable Tasks**: Add a `subItems` property to `BaseTask`. If populated, `TaskSelectionPage.qml` shows a "Review" button to open a Kirigami OverlaySheet where the user can uncheck specific items (like ghost config folders) before hitting "Confirm Tasks".
- **UI Bindings**: Display the calculated size next to each sub-task in `TaskSelectionPage.qml`.

### 6. External Icons
- Create `icons/` directory in the project root.
- Generate high-quality PNG assets using AI:
    - `app-main.png`
    - `clean.png`
    - `maintain.png`
    - `settings.png`
    - `home.png`
    - `success.png`, `error.png`, `warning.png`
- Update QML files (`main.qml`, `LandingPage.qml`, etc.) to use `source: "../../icons/file.png"` instead of `icon.name` or `source: "system-icon"`.
- Update `tasks_config.json` to reference local icon paths for categories.

## Verification Plan
### Manual Verification
1. Run `python main.py`.
2. Verify Landing Page shows actual RAM and Storage values using Python standard libraries.
3. Test task execution to ensure `pkexec` prompts successfully execute when selecting Clean/Maintain tasks.
4. Verify the expandable taskbar works correctly.
5. Verify that all icons load correctly from the `icons/` folder and that no system icons are left in the main UI paths.
