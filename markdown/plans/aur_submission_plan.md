# AUR Submission Plan for AppImage Manager

Prepare the release package (`appimagemanager`) and the VCS package (`appimagemanager-git`) for submission/update on the Arch User Repository (AUR) for the v2.0.0 release.

## User Review Required

> [!IMPORTANT]
> Uploading to the AUR requires authenticating with your AUR account via SSH. As an AI assistant, I cannot access your SSH keys or interact directly with the `aur.archlinux.org` server. I will configure the PKGBUILDs and generate the `.SRCINFO` files locally, then provide you with exact commands to clone, commit, and push the files to the AUR.

> [!NOTE]
> Ensure you have an active account on the [AUR (Arch User Repository)](https://aur.archlinux.org) and have added your SSH public key to your profile settings.

## Proposed Changes

### Packaging - AUR Files

We will clean up the dependencies in both PKGBUILDs to match the current Arch package naming conventions for KDE Frameworks 6 (`kcoreaddons`, `ki18n`, etc.) rather than using obsolete prefixed names like `kf6-coreaddons`.

---

#### [MODIFY] [PKGBUILD (release)](file:///home/herman/Documents/Project/AppImageManager/packaging/aur/appimagemanager/PKGBUILD)
- Correct the dependencies list to use the standard Arch packages.
- Ensure version is set to `2.0.0`.
- Update `sha256sums` with the actual hash of the v2.0.0 tag source archive.

#### [NEW] [.SRCINFO (release)](file:///home/herman/Documents/Project/AppImageManager/packaging/aur/appimagemanager/.SRCINFO)
- Metadata file required by the AUR, generated from the updated PKGBUILD using `makepkg --printsrcinfo`.

---

#### [MODIFY] [PKGBUILD (git)](file:///home/herman/Documents/Project/AppImageManager/packaging/aur/appimagemanager-git/PKGBUILD)
- Correct the dependencies list to use the standard Arch packages.

#### [NEW] [.SRCINFO (git)](file:///home/herman/Documents/Project/AppImageManager/packaging/aur/appimagemanager-git/.SRCINFO)
- Metadata file required by the AUR, generated from the git PKGBUILD using `makepkg --printsrcinfo`.

---

#### [NEW] [aur_submission_plan.md](file:///home/herman/Documents/Project/AppImageManager/markdown/plans/aur_submission_plan.md)
- A copy of this plan saved inside the project's repository as a plan backup, following the rule specified in [GEMINI.md](file:///home/herman/Documents/Project/AppImageManager/GEMINI.md).

## Verification Plan

### Automated / Command-Line Verification
1. Run `namcap` on the PKGBUILDs to lint them for dependency or formatting issues.
2. Run `makepkg --printsrcinfo` to ensure `.SRCINFO` compiles successfully.
3. Perform a dry-run local build with `makepkg -s` to verify CMake builds the binaries and installs them correctly.

### Manual Verification
1. Verify that the output files (`PKGBUILD` and `.SRCINFO`) are placed in their respective directories ready to be pushed.
2. Follow the instructions to push to AUR and confirm they are successfully indexed.
