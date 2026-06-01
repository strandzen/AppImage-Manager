# Implementation Plan - Enhancements (Theme, Selection, Custom Flatpaks)

The goal is to enhance the existing installer with better system integration (theme), more control (selection), and extensibility (adding flatpaks).

## User Review Required
> [!NOTE]
> - By removing the forced "Fusion" style, the app should respect the `QT_STYLE_OVERRIDE` or system configuration. If the user is running this in a pure environment without KDE platform plugins for Qt6, it might look like the default Windows/Fusion style anyway, but this is the correct way to "inherit" if the environment is set up right.

## Proposed Changes

### [GUI & Core]
#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Installer/main.py)
- Remove `app.setStyle("Fusion")`.
- Allow the default system theme to override.

#### [MODIFY] [gui.py](file:///home/herman/Documents/Project/Installer/gui.py)
- **MainWindow Class**:
    - Update `QListWidget` initialization:
        - Enable `itemChanged` signal handling (optional, or just read state on start).
        - For each item, set `flags` to include `ItemIsUserCheckable`.
        - Set default state to `Checked`.
    - **New UI Elements**:
        - Add a `QLineEdit` placeholder "com.example.app".
        - Add `QPushButton` "Add Flatpak".
    - **New Methods**:
        - `add_flatpak()`: Reads text, validates (basic non-empty), creates item with command `flatpak install flathub <id> -y`, and adds to list as checked.
        - `get_selected_tasks()`: Iterates through list items, returns only those with `Qt.CheckState.Checked`.
    - **Update Logic**:
        - `start_installation` should call `get_selected_tasks()` instead of using `self.tasks` directly.

#### [MODIFY] [logic.py](file:///home/herman/Documents/Project/Installer/logic.py)
- No major changes needed, just serving the initial list.

## Verification Plan

### Manual Verification
1. **Theme**: Run on KDE. Verify it looks native (Breeze).
2. **Selection**:
    - Uncheck some items.
    - Click Confirm.
    - Verify only checked items appear in the Terminal log.
3. **Add Flatpak**:
    - Type `org.kde.kcalc` (or similar) in the input.
    - Click Add.
    - Verify it appears in the list (checked).
    - Click Confirm.
    - Verify `flatpak install flathub org.kde.kcalc -y` is executed.
