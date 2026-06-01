# Visual Identity: Local Icons

The application now uses a completely custom, localized icon set. We have moved away from system themes to ensure a consistent, premium look regardless of the user's icon pack.

## New Local Assets
All icons are now stored in the `icons/` folder:

````carousel
![App Logo](file:///home/herman/Documents/Project/Maintainer/icons/app-main.png)
<!-- slide -->
![Clean Category](file:///home/herman/Documents/Project/Maintainer/icons/clean.png)
<!-- slide -->
![Maintain Category](file:///home/herman/Documents/Project/Maintainer/icons/maintain.png)
<!-- slide -->
![Settings](file:///home/herman/Documents/Project/Maintainer/icons/settings.png)
<!-- slide -->
![Home](file:///home/herman/Documents/Project/Maintainer/icons/home.png)
````

## Implementation Highlights
- **PNG Integration**: All icons are high-resolution PNGs with transparency.
- **Dynamic Headers**: The category headers in the task selection page now dynamically load the correct local icon pass-through.
- **Finished States**: Success, Error, and Warning states in the final results page now use the new custom assets.

### File Changes
- [main.qml](file:///home/herman/Documents/Project/Maintainer/qml/main.qml): Updated sidebar and navigation actions.
- [LandingPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/LandingPage.qml): Updated main landing page hero icon.
- [TaskSelectionPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/TaskSelectionPage.qml): Added dynamic category icon property.
- [FinishedTasksPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/FinishedTasksPage.qml): Updated task result markers.
- [tasks_config.json](file:///home/herman/Documents/Project/Maintainer/tasks_config.json): Updated category icons to point to `./icons/`.
