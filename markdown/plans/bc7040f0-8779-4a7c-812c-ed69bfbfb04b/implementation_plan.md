# AppImage Compatibility Fix Plan

## Goal Description
Fix AppImage failures on Fedora Kinoite (and other distros) by:
1.  Lowering the `glibc` requirement (by building on Ubuntu 20.04).
2.  Addressing FUSE compatibility (ensuring the AppImage works on systems with only FUSE 3 or missing FUSE 2).

## User Review Required
> [!IMPORTANT]
> The build process will now run inside a container (Podman). This requires `podman` to be installed and runnable by the user (which verified it is).

## Proposed Changes

### Build Infrastructure
#### [NEW] [Containerfile](file:///home/herman/Documents/Project/Installer/Containerfile)
- Create a container definition based on `ubuntu:20.04`.
- Install necessary build dependencies:
  - python3.9 (or newer if needed, managed via deadsnakes PPA or similar)
  - pip
  - binutils (for `objcopy`, `ld`)
  - file
  - fuse (for runtime testing, optional)
  - patchelf
  - AppImageKit / linuxdeploy tools

#### [NEW] [build_appimage.sh](file:///home/herman/Documents/Project/Installer/build_appimage.sh)
- Script to:
  1.  Build the container image.
  2.  Run the container with the source code mounted.
  3.  Execute the build commands (pip install, pyinstaller/linuxdeploy).
  4.  Output the final AppImage to the host.

### FUSE Compatibility
- We will use `linuxdeploy` with a modern runtime that attempts to be more resilient, or instruct the user on the `APPIMAGE_EXTRACT_AND_RUN=1` environment variable if FUSE is completely missing.
- *Investigation note*: Newer `linuxdeploy` releases often include a runtime that tries to `dlopen` libfuse2 or libfuse3, or falls back. We will ensure we use a recent version of `linuxdeploy`.

## Verification Plan

### Automated Tests
- Run the build script: `./build_appimage.sh`
- Verify the output AppImage exists.
- Check `glibc` requirement of the binary inside the AppImage (using `objdump` or `strings` on the generated binary inside the container).

### Manual Verification
- User to copy the generated AppImage to their Fedora Kinoite system (or use `podman run -it fedora:latest` to simulate it if possible, though GUI apps are hard to test in headless containers).
- Run with `./My.AppImage`.
