# Implementation Plan - Immutable Distro Support

This plan adapts the installer for immutable distributions (like Fedora Silverblue or SteamOS) by shifting Flatpak operations to user-space and allowing the user to bypass `sudo` requirements.

## Proposed Changes

### Logic Component
#### [MODIFY] [logic.py](file:///home/herman/Documents/Project/Installer/logic.py)
- **Flatpak User-space**:
    - Update `ensure_flathub_command` to include the `--user` flag.
    - Update `generate_install_plan` to use `flatpak install --user` for all application installs.
- **Improved Detection**:
    - Ensure `detect_package_manager` handles scenarios where a system package manager might not be interactive or available (though it currently errors out, we can make it more lenient if in Immutable Mode).

### GUI Component
#### [MODIFY] [gui.py](file:///home/herman/Documents/Project/Installer/gui.py)
- **Password Dialog**:
    - Add a new button: "Immutable Distro / No Sudo".
    - Clicking this button will set a flag (e.g., `self.is_immutable = True`) and accept the dialog.
- **Main Window**:
    - Add logic to check if "Immutable Mode" is active.
    - If active, permanently remove the **System** category from the task tree to prevent failed `sudo` calls.

### Execution Flow
#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Installer/main.py)
- Update the dialog handling to pass the `is_immutable` flag to the `MainWindow`.

## Verification Plan

### Manual Verification
1. Run the app: `python3 main.py`.
2. On the Password Dialog:
    - Click "Immutable Distro / No Sudo".
3. Verify the Main Window:
    - The **System** category should be hidden.
    - **Flatpaks** and **Other Apps** sections should remain visible.
4. Verify Command Generation:
    - Hover over a Flatpak to see its tool-tip; it should now show `flatpak install --user`.
5. Verify Installation:
    - Proceed with an installation and verify no sudo password is requested for Flatpaks.
