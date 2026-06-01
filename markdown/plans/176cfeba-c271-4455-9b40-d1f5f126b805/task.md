# Fix Corpse Cleaner Functionality

- [x] Investigate why cleaning selected files does not work.
- [x] Investigate why the UI list does not refresh after cleaning.
- [x] Fix the corpse cleaner logic.
- [x] Verify the fixes.

# Scalable and Resizable Panes

- [x] Make the left pane scalable to fit the icon + name.
- [x] Make the right pane resizable horizontally by dragging.
- [x] Ensure scaling uses `SplitView` or Kirigami configurations appropriately.
- [x] Integrate compact mode for the right Action Queue pane.

# System Monitor Enhancements

- [x] Add scrollbars to the Applications list in the monitor page.
- [x] Make the Applications list more advanced (display PID, etc., if available).
- [x] Add options to kill a process and show it in explorer (e.g., via a context menu or inline buttons).
- [x] Convert process context menu into separate inline buttons (open folder, kill process).
- [x] Add a visual search bar to the Applications list to filter processes.

# Corpse Cleaner Enhancements

- [x] Add a visual search bar to the Corpse Cleaner list to filter corpses.

# UI and Icon Updates

- [x] Use `sudo.svg` as a visual flag in task listings for items requiring superuser access.
- [x] Add custom hover tooltips to Recommended, Advanced, and Sudo flags explaining their purpose.
- [x] Update `kill.svg` and `ssd.svg` to inherit correct icon color styles instead of rendering black.
- [x] Bind tooltips exclusively to explicit hovered icons rather than the entire list row.
- [x] Automatically expand right-side lists (Action Queue/Results) to accommodate the longest text entry across all pages.

# Left Panel Navigation Organizer

- [x] Create `ui_left_list.json` to define sidebar structure and order.
- [x] Implement python backing store (e.g. `SidebarModel`) to map JSON objects dynamically to UI list requirements.
- [x] Connect the "volatile" list visibility state to the user settings toggle.
- [x] Completely migrate `main.qml` sidebar to iterate over the new model instead of hardcoding routes.
- [x] Rename volatile state to `hidden`, replace Simulation Mode toggle with Developer Mode.
- [x] Fix sidebar items sticking to the bottom of the window by nesting the `Instantiator` in a `ColumnLayout` above the trailing Spacer.
- [x] Fix sidebar selection boxes not filling available horizontal space.
- [x] Fix sidebar Dividers / Separators rendering invisibly. 
- [x] Fix Developer Mode toggle ignored by `main.qml` list instantiator.

# Icon Coloring Control

- [x] Add `custom_colors` map to `ui_icons.json` to allow icon-specific styling overrides.
- [x] Expose an `iconColor(key)` or `customColors` method via `UIIconsManager` to QML.
- [x] Refactor QML icons (like recommended, advanced, sudo) to fetch color dynamically from `ui_icons` falling back to Kirigami default.
