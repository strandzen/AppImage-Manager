# Rebuild Binary with PyInstaller

Rebuild the `ThemeSwitcher` binary to ensure it's up to date with the latest code changes.

## Proposed Changes

### [PyInstaller Configuration]

#### [MODIFY] [ThemeSwitcher.spec](file:///home/herman/Documents/Project/Konsave/ThemeSwitcher.spec)
- Remove `Previews` data dependency if it remains missing and is not needed by the script.

## Verification Plan

### Automated Tests
- Build the binary using PyInstaller:
  ```bash
  ./build_venv/bin/pyinstaller ThemeSwitcher.spec --clean --noconfirm
  ```
- Check if `./dist/ThemeSwitcher` exists.
- Run the binary to ensure it starts (if possible in this environment, otherwise just check existence).
