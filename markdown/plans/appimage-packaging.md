# AppImage Packaging Plan

## What was done

- Added `option(BUILD_DOLPHIN_PLUGIN "..." ON)` to `src/CMakeLists.txt` — wraps the `kcoreaddons_add_plugin` block so passing `-DBUILD_DOLPHIN_PLUGIN=OFF` skips the `.so` entirely.
- Created `packaging/build-appimage.sh` — full end-to-end script.

## Script flow

1. Downloads `linuxdeploy` + `linuxdeploy-plugin-qt` to `build/appimage/tools/` if not present.
2. Configures with `-DBUILD_DOLPHIN_PLUGIN=OFF -DBUILD_TESTING=OFF`, installs to `build/appimage/AppDir/`.
3. **Manually bundles KDE QML modules** that linuxdeploy-plugin-qt doesn't know about:
   - `org/kde/kirigami`
   - `org/kde/kirigamiaddons`
   - `Qt/labs/platform`
4. Writes a custom `AppRun` that sets `LD_LIBRARY_PATH`, `QT_PLUGIN_PATH`, `QML2_IMPORT_PATH`, `XDG_DATA_DIRS`.
5. Copies `.desktop` file and icon to AppDir root (required by linuxdeploy).
6. Runs linuxdeploy with `--plugin qt` to deploy Qt libraries and standard plugins.
7. Outputs `AppImageManager-x86_64.AppImage` in the project root.

## Known limitations / caveats

- **KDE platform theme**: The app may not pick up the host system's Plasma theme inside the AppImage. Text and controls will still work but may look plain on non-KDE desktops.
- **libappimage.so**: Must be present on the build host; linuxdeploy will bundle it from the installed path. If it was built from source and installed to `/usr/local`, ensure `LD_LIBRARY_PATH` includes `/usr/local/lib` at packaging time.
- **FUSE nesting**: The AppImage runtime uses FUSE2 to mount itself. libappimage's squashfuse fallback also uses FUSE. Most kernels allow nested FUSE, but if the AppImage is run inside a container without `--device /dev/fuse`, the squashfuse fallback will fail. The primary libappimage path (in-process SquashFS) does not require FUSE.
- **Architecture**: Script hardcodes `x86_64`. For other arches, change the linuxdeploy download URLs and output filename.
- **Qt version**: `qmake6` must be on PATH at packaging time. On systems where it's called `qmake`, the `QMAKE=qmake6` env var passed to linuxdeploy may need adjusting.

## Usage

```bash
./packaging/build-appimage.sh
```
