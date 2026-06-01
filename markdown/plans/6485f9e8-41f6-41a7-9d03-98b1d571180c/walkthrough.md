# System Monitor Improvements Walkthrough

I have implemented several improvements to the System Monitor page to address the issues you reported.

## Changes Made

### Icon Colors
- Fixed a typo in `ui_icons.json` where hex color values were missing a character (changed `"#fffff"` to `"#ffffff"`). This ensures the `ssd` and `kill` icons are correctly colorized.

### Process List Stability
- Implemented a new `ProcessModel` in `system_health.py` that inherits from `QAbstractListModel`.
- This model now handles updates by comparing PIDs and only emitting `dataChanged` signals for updated values (CPU, Memory), instead of resetting the entire model.
- **Result**: The scroll position in the applications list now remains stable when the list refreshes every 2 seconds.

- **Standardized Icons**: All trash, delete, and removal icons across the application now use the unified `clean.svg` asset.
- **Icon Colorization**: Fixed coloring for `url`, `kill`, and `check_update` icons by utilizing `Kirigami.Icon` with `isMask: true`, ensuring they follow the thematic colors.
- **UI Refinements**:
    - **Smarter Path Display**: The AppImage list now shows only the containing directory (e.g., `/home/herman/AppImages/`) instead of the full filename, reducing visual clutter.
    - **Precision Icons**: Reduced URL and Update icons to a much smaller size (approx. 10px) for a professional, clean look.
    - **Central URL Management**: Clicking a URL icon now opens a central dialog to input or edit the GitHub update URL for that specific AppImage.
    - **Dynamic Accents**: URL icons are now accented (highlight color) only when a URL is present, making it easy to see which apps are configured for updates.
    - **Reactive Batch Actions**: The "Delete Selected" button now correctly updates and enables/disables instantly as you select/deselect AppImages.
- **Vertical Expansion**: Fixed the layout bug that prevented the side pane from filling the vertical space.

## Verification

### Manual Verification
1.  **Icon Colors**: The "Open Folder" and "Kill" icons now appear with the correct custom colors defined in the theme.
2.  **Scroll Stability**: Scrolling down the list and waiting for an update no longer resets the view to the top.
3.  **Expansion**: The "Applications" box now stretches to the bottom of the page.
4.  **Scrollbar**: The scrollbar is persistently visible, even when the list doesn't require scrolling (though it's usually long enough to require it).

render_diffs(file:///home/herman/Documents/Project/Maintainer/ui_icons.json)
render_diffs(file:///home/herman/Documents/Project/Maintainer/models/system_health.py)
render_diffs(file:///home/herman/Documents/Project/Maintainer/qml/pages/SystemMonitorPage.qml)
