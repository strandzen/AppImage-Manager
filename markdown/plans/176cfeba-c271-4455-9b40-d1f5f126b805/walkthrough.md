# Walkthrough: Developer Mode & Icon Coloring

I have completed the implementation of Developer Mode and the centralized icon coloring system.

## Key Changes

### 1. Developer Mode
- **Renamed** all "volatile" and "simulation" terminology to `Developer Mode` and `hidden` across JSON configs, Python models, and QML.
- **Filtering Logic**: `SidebarModel` and `TaskRegistry` now exclusively listen to the `developerMode` setting to filter items marked with `"hidden": true`.
- **UI Toggle**: Updated the Settings page with the new label and tied it to the backend setting.

### 2. Sidebar Layout Fixes
- **Top Alignment**: Fixed the issue where sidebar items were pushed to the bottom by nesting the `Repeater` in a layout above the trailing spacer.
- **Horizontal Expansion**: Replaced `Instantiator` with `Repeater` and used `parent.width` bindings to ensure selection highlights and separators fill the entire sidebar width.

### 3. Centralized Icon Coloring
- **`ui_icons.json`**: Added a `custom_colors` map for fine-grained per-icon coloring.
- **Python Helper**: Added `iconColor(key, fallback)` to `UIIconsManager` to resolve colors based on:
  1. Custom override in JSON.
  2. Global `defaultColor` in JSON.
  3. Optional theme fallback provided by QML.
- **Global Refactor**: Updated every `Kirigami.Icon` in the app to use this helper, removing hardcoded theme colors.

## Verification
- [x] Toggle Developer Mode: Items instantly appear/disappear.
- [x] Sidebar layout: Highlighting fills horizontal space, items are top-aligned.
- [x] Icon colors: `sudo`, `recommended`, and `advanced` now use the custom hex codes from `ui_icons.json`.

render_diffs(file:///home/herman/Documents/Project/Maintainer/ui_icons.json)
render_diffs(file:///home/herman/Documents/Project/Maintainer/ui_left_list.json)
render_diffs(file:///home/herman/Documents/Project/Maintainer/qml/main.qml)
