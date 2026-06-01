# Walkthrough: Rebuild Binary with PyInstaller

I have successfully rebuilt the `AppCenter` binary using PyInstaller.

## Changes Made
- Executed `pyinstaller` using the existing `installer-widget.spec` configuration.
- Verified the build output in the `dist/` directory.

## Verification Results

### Build Verification
I verified that the binary was created successfully:

```bash
ls -l dist/AppCenter
-rwxr-xr-x 96M herman 24 Jan 16:28 dist/AppCenter
```

The binary is approximately 96MB in size and has executable permissions.

### Update: Flatpak Uninstall Fix
I fixed an issue where uninstalling Flatpaks failed due to an invalid command format. The `flathub` remote name is now correctly stripped from `uninstall` and `update` commands.

Verified results:
```bash
Remove Input: flatpak install --user flathub io.mpv.Mpv -y
Remove Result: flatpak uninstall io.mpv.Mpv -y

Upgrade Input: flatpak install --user flathub io.mpv.Mpv -y
Upgrade Result: flatpak update io.mpv.Mpv -y
```
