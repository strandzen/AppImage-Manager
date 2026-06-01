# AppImage Manager — GitHub Integration

A full redesign of the AppImage Manager to support GitHub-linked version tracking, update discovery, and in-app update execution.

## Proposed Changes

---

### Backend — `models/appimage_manager.py`

#### [MODIFY] [appimage_manager.py](file:///home/herman/Documents/Project/Maintainer/models/appimage_manager.py)

- **Version Parsing**: Add `_parse_version_from_filename(name)` → uses regex to extract semver from filename (e.g. `AppName-1.2.3-x86_64.AppImage` → `1.2.3`).
- **Signals**: Add `selectedAppImageChanged`, `updateCheckFinished`, `updateDownloadProgress(float)`, `updateDownloadFinished`.
- **Properties**:
  - `selectedAppImage: var` — object holding the currently selected item.
  - `isChecking: bool` — True while any background check is running.
  - `isDownloading: bool` — True while a download is in progress.
  - `downloadProgress: float` (0.0–1.0).
- **Async GitHub API**: Use `QNetworkAccessManager` + `QNetworkReply` for all HTTP calls (no `requests` library on main thread). `check_all_updates()` iterates the model and fires per-item API requests.
- **`check_for_updates(path)`**: Per-item update check → sets `update_status` to `"checking"`, fires off a network request, and on completion sets `"available"`, `"up-to-date"`, or `"error"`.
- **`download_update(path)`**: Downloads the latest `.AppImage` asset from GitHub releases → streams to a temp file → on completion: `chmod +x`, deletes old file, moves new file → calls `scan()`.
- **`select_appimage(path)`**: Sets `selectedAppImage` property for the center pane.
- **`checkedCount` signal**: Already fixed; ensure it remains `checkedCountChanged`.
- **Model Roles**: Add `ReleaseNotesRole` (`releaseNotes`, stores release body text).

---

### Center Pane — `qml/pages/AppImageManagerPage.qml`

#### [MODIFY] [AppImageManagerPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/AppImageManagerPage.qml)

Full rewrite of the center pane to show:
- **No selection**: A styled placeholder message ("Select an AppImage to see details").
- **With selection**:
  - App name (large heading).
  - Current version (parsed from filename).
  - Latest version from GitHub (with colored status badge: `up-to-date` = green, `available` = amber, `error` = red, `idle`/`checking` = neutral).
  - Release notes in a `ScrollView` with a `TextArea` below the versions.
- Page-level action buttons in the toolbar: "Refresh" (re-scan) and "Check All" (trigger `check_all_updates()`).

---

### Right Side Pane — `qml/components/TaskProgressPane.qml`

#### [MODIFY] [TaskProgressPane.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/TaskProgressPane.qml)

Refine the AppImage-specific section:
- **List item changes**:
  - Fix `ToolButton` binding loop warnings by setting explicit `implicitWidth`/`implicitHeight`.
  - Add a colored status dot/icon next to each entry based on `updateStatus`.
  - When list item is clicked/tapped, call `AppImageManager.select_appimage(model.path)`.
- **Action buttons** (AppImage state):
  - **State A** (no selection or no update available): "Check for Updates" button → calls `AppImageManager.check_all_updates()`.
  - **State B** (selection with `updateStatus === "available"`): "Update" button → calls `AppImageManager.download_update(...)`.
  - **Persistent**: "Delete (n)" button below, enabled only when checked count > 0.
  - **Progress**: A `ProgressBar` + status label that appears during download.

---

### New Helper QML Component — `qml/components/VersionBadge.qml`

#### [NEW] [VersionBadge.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/VersionBadge.qml)

A small reusable component that shows a colored badge/chip for update status (idle, checking, available, up-to-date, error).

---

## Verification Plan

### Automated Checks
```bash
# Run the app and verify no Python exceptions
/bin/python /home/herman/Documents/Project/Maintainer/main.py
```

### Manual Verification
1. Open AppImage Manager page → list populates from disk.
2. Click an AppImage → center pane shows version info and placeholder release notes.
3. Click URL icon → set a GitHub URL (e.g. `https://github.com/mifi/lossless-cut`).
4. Click "Check for Updates" → status badge updates; release notes appear for available updates.
5. If update available, click "Update" → progress bar appears, file is swapped, scan re-runs.
6. Check the "Delete" button → is always visible but disabled when nothing is checked.
