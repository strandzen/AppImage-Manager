# Replace Restart Button with Save Profile Feature

This plan outlines the steps to remove the "Restart Plasma" button and replace it with a "Save current" button that prompts the user for a profile name and executes `konsave -s`.

## Proposed Changes

### Konsave Application

#### [MODIFY] [themeSwitcher](file:///home/herman/Documents/Project/Konsave/themeSwitcher)

- Import `QInputDialog` from `PyQt6.QtWidgets`.
- Remove `self.restart_btn` and its layout addition.
- Add `self.save_current_btn` with text "Save current".
- Connect `self.save_current_btn.clicked` to a new method `save_current_action`.
- Implement `save_current_action(self)`:
    - Open a text input dialog to get the profile name.
    - Validate if the name is not empty.
    - Check if the name already exists in `self.profiles`.
    - If exists, show a confirmation dialog for overwriting.
    - If user confirms or it's a new profile:
        - Construct command: `konsave -s <name> [--force]`.
        - Execute command via `subprocess.run`.
        - Create a `README.md` file in `~/.config/konsave/profiles/<name>/README.md` with "Custom profile, saved by user".
        - Refresh the profile list.
- Remove the `restart_plasma` method.

## Verification Plan

### Automated Tests
- N/A (Manual UI verification required)

### Manual Verification
- Launch the application: `./themeSwitcher`.
- Verify the "Restart Plasma" button is gone.
- Verify the "Save current" button is present.
- Click "Save current" and enter a new name (e.g., "TestProfile").
- Verify `konsave -s TestProfile` is executed.
- Verify `~/.config/konsave/profiles/TestProfile/README.md` is created correctly.
- Verify the list refreshes and shows "TestProfile".
- Try saving again with the same name and verify the overwrite prompt appears.
- Verify that choosing "Yes" to overwrite runs `konsave -s TestProfile --force`.
