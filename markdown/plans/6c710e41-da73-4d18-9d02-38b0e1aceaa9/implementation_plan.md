# Plan: Refined Uninstallation Flow

Enhance the uninstallation process by integrating the dry run preview, sudo prompt, and real-time progress reporting within a single modal experience.

## User Review Required
> [!IMPORTANT]
> - The confirmation `OverlaySheet` will stay open during the removal process to show live progress.
> - A sudo prompt (GUI) will appear when the actual removal starts.
> - The sheet will automatically close upon successful completion, followed by a list refresh.

## Proposed Changes

### UI Components
#### [MODIFY] [PackageManagerPane.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/PackageManagerPane.qml)
- Add a `state` or boolean tracking `isRemoving` within the `OverlaySheet`.
- Replace the preview content with a progress indicator and terminal-style output (from `PackageManager.progressText`) once removal starts.
- Disable "Cancel" and "Uninstall Now" buttons during the process.
- Use `Connections` to detect when removal is finished and automatically `close()` the sheet.

### Backend (Package Manager)
#### [MODIFY] [package_manager.py](file:///home/herman/Documents/Project/Maintainer/models/package_manager.py)
- Ensure `RemovePackageWorker` emits enough detail for the UI to show meaningful progress.
- Verify `isRemoving` state management.

## Verification Plan
### Manual Verification
- Select a package, click "Remove", verify the preview sheet appears.
- Click "Uninstall Now", enter sudo password in the OS prompt.
- Observe progress within the sheet.
- Verify the sheet closes and the list refreshes once done.
