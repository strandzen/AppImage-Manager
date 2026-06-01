# PyInstaller Build Plan

The user has explicitly requested to rebuild the application using `PyInstaller` and place the output in the `/dist` folder.

## Proposed Changes

### Build Execution
- Use `PyInstaller` with the existing `ThemeSwitcher.spec` file.
- Command: `./build_env/bin/pyinstaller ThemeSwitcher.spec`

## Verification Plan

### Automated Tests
- Check exit code of PyInstaller command.
- Verify existence of `dist/ThemeSwitcher`.
- Check file type of `dist/ThemeSwitcher` (should be ELF executable).

### Manual Verification
- N/A
