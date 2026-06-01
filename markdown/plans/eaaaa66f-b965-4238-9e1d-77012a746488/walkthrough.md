# Build Walkthrough

## Completed Work
Built the App Center installer binary using PyInstaller.

## Changes
- **Environment**: Installed `pyinstaller` in the local `.venv`.
- **Build**: executed `pyinstaller installer-widget.spec`.

## Verification Results
### Automated Tests
- N/A

### Manual Verification
- Verified presence of `dist/AppCenter` binary (96MB).

## Debugging VM Issues
### Issues Fixed
1. **Theme Switcher Download**: Replaced fragile `grep`/`cut` command with robust Python JSON extraction.
2. **App Detection**: Updated `flatpak list` to use `--columns=application` for reliable ID parsing.
3. **Okular Management**: Fixed a bug where "Manage" view triggered "Install" view logic due to mode mismatch.

### Verification
- **Rebuild**: Rebuilt the binary to include these fixes.

### Additional Fixes
- **Manage Selection Bug**: Fixed an issue where selecting an installed app in "Manage" mode showed a "No Selection" error because the code was incorrectly checking for "(Installed)" in the text instead of using internal data tags.
- **Flatpak Uninstall Ambiguity**: Modified the uninstall logic to explicitly target both `--user` and `--system` installations (e.g., `flatpak uninstall --user ...; flatpak uninstall --system ...`). This prevents failures when an app is installed in both locations, which previously caused `flatpak` to prompt for input and fail in non-interactive mode.

