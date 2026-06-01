# Enhancements: Context Menu & Force Overwrite

Implement a right-click context menu to open the profiles folder and ensure "Get Latest" overwrites existing files.

## Proposed Changes

### [themeSwitcher](file:///home/herman/Documents/Project/Konsave/themeSwitcher)

#### [MODIFY]

- **Context Menu**:
    - Enable `CustomContextMenu` policy on `self.list_widget`.
    - Connect `customContextMenuRequested` signal to a new slot `show_context_menu`.
    - Implement `show_context_menu`:
        - Create a `QMenu`.
        - Add "Open Profiles Folder" action.
        - Open `PROFILES_DIR` using `QDesktopServices.openUrl(QUrl.fromLocalFile(PROFILES_DIR))`. (Requires updating imports).
- **Force Overwrite**:
    - Update `get_latest_action` to use `cp -rf` instead of `cp -r`.

## Verification Plan

### Manual Verification
1. Run the app.
2. Right-click on the list (or empty space in list).
3. Select "Open Profiles Folder" and verify the file manager opens.
4. functionality of "Get Latest" remains, now explicitly forcing overwrites.
