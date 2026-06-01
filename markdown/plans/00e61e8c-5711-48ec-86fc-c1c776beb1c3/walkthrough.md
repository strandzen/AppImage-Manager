# Theme-Switcher Integration Walkthrough

I have integrated the Theme-Switcher installation into the system installer.

## Changes Made

### Logic & UI Layer

- Added `check_theme_switcher` to [logic.py](file:///home/herman/Documents/Project/Installer/logic.py#L113-L125) which constructs the shell command for downloading and installing the Theme-Switcher binary.
- Added `check_ferdium` to [logic.py](file:///home/herman/Documents/Project/Installer/logic.py#L126-L136) which constructs the shell command for downloading and installing the Ferdium AppImage.
- Updated `generate_install_plan` in [logic.py](file:///home/herman/Documents/Project/Installer/logic.py#L159-L179) to include Theme-Switcher and Ferdium tasks before the Affinity installation.
- **List Sorting & Grouping**: [gui.py](file:///home/herman/Documents/Project/Installer/gui.py#L220-L233) now sorts the UI list alphabetically and groups `(flathub)` items together at the bottom, while preserving original installation order during execution.
- **Refined Task Descriptions**: Updated component list to use more concise names with source tags (e.g., `com.spotify.Client (flathub)` instead of `Install Flatpak: com.spotify.Client`).

## Proof of Work

### Code Inspection

The `check_theme_switcher` method correctly implements the provided script logic:

```python
    def check_theme_switcher(self):
        repo = "strandzen/Theme-Switcher"
        dest_dir = "$HOME/Applications"
        return (
            f"mkdir -p {dest_dir} && "
            f"DW_URL=$(curl -s https://api.github.com/repos/{repo}/releases/latest | grep 'browser_download_url' | cut -d : -f 2,3 | tr -d \\\" | head -n 1) && "
            f"FILE_NAME=$(basename \"$DW_URL\") && "
            f"curl -L \"$DW_URL\" -o \"/tmp/$FILE_NAME\" && "
            f"mv \"/tmp/$FILE_NAME\" {dest_dir}/ && "
            f"chmod +x {dest_dir}/$FILE_NAME"
        )
```

The task list now shows Theme-Switcher before Affinity:

```python
        # 7. Theme-Switcher
        tasks.append(("Install Theme-Switcher", self.check_theme_switcher()))

        # 8. Affinity
        tasks.append(("Install Affinity", self.check_affinity()))
```

### Verification Results

The changes were verified by checking the syntax of `logic.py` and reviewing the generated command structure. The installer now presents "Install Theme-Switcher" as a component to the user.

## Binary Build & UI Reversal

I have successfully rebuilt the standalone binary and reverted the custom UI styling.

- **UI Style**: Reverted to inheriting the system theme (KDE Breeze).
- **Terminal Styling**: Restored original hardcoded terminal colors in `gui.py`.
- **Command**: `pyinstaller --noconfirm installer-widget.spec`
- **Output Binary**: `dist/installer-widget` (Size: 89MB)

The application now correctly inherits your system theme as originally requested.
