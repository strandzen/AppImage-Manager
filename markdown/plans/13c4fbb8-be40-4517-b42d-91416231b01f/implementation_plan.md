# UI Design System: Persistent Gauges & Standardized Headers

The goal is to refine the central UI by removing borders, making health gauges (SSD, RAM, Storage) persistent across all pages, and implementing a standardized header system (Scalable SVG Icon + Headline + Description Box) managed through `ui_strings.json` and `ui_icons.json`.

## Proposed Changes

### Assets
#### [NEW] [ui_icons.json](file:///home/herman/Documents/Project/Maintainer/ui_icons.json)
- Define all icon paths, names, and default properties (size, global options).

#### [NEW] Additional SVGs
- [ssd.svg](file:///home/herman/Documents/Project/Maintainer/icons/ssd.svg)
- [ram.svg](file:///home/herman/Documents/Project/Maintainer/icons/ram.svg)
- [storage.svg](file:///home/herman/Documents/Project/Maintainer/icons/storage.svg)
- [app-main.svg](file:///home/herman/Documents/Project/Maintainer/icons/app-main.svg) (Created/Redirected)

#### [DELETE] Legacy Assets
- Remove all `*.png` files from `icons/` directory.

### Backend
#### [NEW] [ui_icons_manager.py](file:///home/herman/Documents/Project/Maintainer/models/ui_icons_manager.py)
- A Python class to load `ui_icons.json` and expose icon paths and properties to QML.

#### [MODIFY] [main.py](file:///home/herman/Documents/Project/Maintainer/main.py)
- Initialize `UIIconsManager` and expose it as `UIIcons` to the QML context.

### UI Components
#### [MODIFY] [main.qml](file:///home/herman/Documents/Project/Maintainer/qml/main.qml)
- Remove the border/rectangle around the center pane.
- Move the status gauges (SSD, RAM, Storage) from `LandingPage.qml` to the bottom of the central column so they are visible on all pages.

#### [MODIFY] [LandingPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/LandingPage.qml)
#### [MODIFY] [TaskSelectionPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/TaskSelectionPage.qml)
#### [MODIFY] [CorpseCleanerPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/CorpseCleanerPage.qml)
- Implement a standardized header:
    - Large SVG icon (sourced from `UIIcons`, scale controlled via `ui_icons.json`).
    - Headline (sourced from `UIStrings`).
    - Semi-transparent description box (sourced from `UIStrings`).

### Configuration
#### [MODIFY] [ui_strings.json](file:///home/herman/Documents/Project/Maintainer/ui_strings.json)
- Centralize all page headlines and descriptions.

#### [MODIFY] [ui_icons.json](file:///home/herman/Documents/Project/Maintainer/ui_icons.json)
- Add sizing/scale options for "headline" icons.


## Verification Plan

### Automated Tests
- Check that the `UIIconsManager` correctly parses the JSON file.

### Manual Verification
- Run the application and verify all icons load correctly.
- Test that updating a global property in `ui_icons.json` (like a size multiplier) reflects in the UI.
- Verify that no PNG reference remains in the codebase.

