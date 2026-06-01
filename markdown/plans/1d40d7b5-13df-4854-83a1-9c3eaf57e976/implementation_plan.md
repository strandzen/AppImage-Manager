# Plan to Rebuild Binary with PyInstaller

The objective is to create a standalone binary for the newly rebranded **App Center**, ensuring all preview images and data files are bundled correctly.

## Proposed Changes

### Build Configuration

#### [MODIFY] [installer-widget.spec](file:///home/herman/Documents/Project/Installer/installer-widget.spec)
- Update `datas` to include the `previews/` directory.
- Rename the output executable from `installer-widget` to `AppCenter`.
- Ensure `icon.png` and `packages.json` are included.

## Verification Plan

### Automated Steps
- Run `pyinstaller installer-widget.spec`.
- Check if `dist/AppCenter` exists.
- Run the generated binary to ensure it opens and loads assets.
