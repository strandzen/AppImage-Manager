# Walkthrough - Implement Theme Switcher Features

I have updated the `themeSwitcher` with two main features: standardizing the Plasma restart command and adding a confirmation popup after applying a theme.

## Changes

### Theme Switcher

#### [themeSwitcher](file:///home/herman/Documents/Project/Konsave/themeSwitcher)

- **Restart Logic**: Replaced `subprocess.Popen(["plasmashell", "--replace"], ...)` with `subprocess.run(["systemctl", "restart", "--user", "plasma-plasmashell"], ...)`.
- **Popup Logic**: Added `QMessageBox` to `apply_action` to show "Success" or "Error" messages based on the `konsave` command result.
- **Rebuild**: Rebuilt the application using `pyinstaller`.

## Verification Results

### Build Verification
- [x] Confirmed `plasma-plasmashell.service` exists on the system.
- [x] Rebuilt application successfully (`dist/ThemeSwitcher` exists).

### Manual Verification
- [ ] **Restart**: User should verify by clicking "Restart Plasma".
- [ ] **Apply Theme**: User should verify by clicking "Apply Theme" and waiting for the success/error popup.
