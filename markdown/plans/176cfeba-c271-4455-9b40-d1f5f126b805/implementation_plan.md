# Implementing Left Panel Navigation Organizer

The user wants a master file `ui_left_list.json` to organize the left-hand navigation panel. This will allow reordering of all items (both static pages like Home/Monitor and dynamic task categories) and hiding specific items using a "volatile" flag toggled by a setting.

## Proposed Changes

### Configuration Files
#### [NEW] `ui_left_list.json`
Create a new configuration file defining the sidebar items:
```json
[
    { "id": "home", "type": "page", "name": "home", "icon": "home", "url": "pages/LandingPage.qml", "volatile": false },
    { "id": "monitor", "type": "page", "name": "monitor", "icon": "monitor", "url": "pages/SystemMonitorPage.qml", "volatile": false },
    { "id": "settings", "type": "page", "name": "settings", "icon": "settings", "url": "pages/SettingsPage.qml", "volatile": false },
    { "type": "separator" },
    { "type": "label", "name": "Available Tasks" },
    { "id": "clean_system", "type": "category", "volatile": false },
    { "id": "maintain_system", "type": "category", "volatile": false },
    { "id": "custom_scripts", "type": "category", "volatile": false },
    { "id": "corpse_cleaner", "type": "category", "volatile": true },
    { "id": "boot_audit", "type": "category", "volatile": true }
]
```

### Python Backend
#### [MODIFY] `models/settings_manager.py`
- Expose a `showVolatileTasks` property (if one doesn't already exist or repurpose `showSimulationTasks`) with signals.

#### [NEW] `models/sidebar_model.py`
- Create a `SidebarModel` that parses `ui_left_list.json`.
- Expose roles for `id`, `type`, `name`, `icon`, `url`, and `volatile`.
- Automatically translate `name` using `UIStrings` keys for pages.
- Handle categories dynamically to load titles and icons from `TaskRegistry`.
- Watch `SettingsManager.showVolatileTasks` to filter out `volatile` items on the fly.

#### [MODIFY] `main.py`
- Instantiate `SidebarModel` and expose it to the QML scope as `SidebarModel`.

### QML Frontend
#### [MODIFY] `qml/main.qml`
- Remove the hardcoded `SidebarItem`s (Home, Monitor, Settings) and the second `Instantiator` for categories.
- Replace them with a single `Instantiator`/`ListView` driven by `SidebarModel`.
- Create a `DelegateChooser` or conditional `Loader`/`Item` wrapper inside the delegate that dynamically renders:
  - A `Kirigami.Separator` for `model.type === "separator"`
  - A styled `Label` (like the current "Available Tasks") for `model.type === "label"`
  - A `MyComponents.SidebarItem` for `model.type === "page"` or `"category"`
- The `SidebarItem.onClicked` handler accesses `model.type`, `model.url`, or `model.id` to route to static pages vs `TaskSelectionPage`/`CorpseCleanerPage` seamlessly.

## Verification Plan
1. **Manual Testing**: 
   - Start the application. Verify the sidebar items match the absolute order defined in `ui_left_list.json`.
   - Toggle the "Show volatile tasks" setting in the Settings page.
   - Verify that items marked as `"volatile": true` (like Corpse Cleaner or Boot Audit) instantly appear/disappear.
   - Add a new item to `ui_left_list.json` or reorder them, run the app again, and ensure the order adheres perfectly.
