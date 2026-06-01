# Implementation Plan: Remove Action Queue from Home Page

## Proposed Changes

### UI Changes

#### [MODIFY] [main.qml](file:///home/herman/Documents/Project/Maintainer/qml/main.qml)
- Update `rightPaneLoader` visibility to exclude `landingPage`.
- Update `SplitView.fillWidth` and `SplitView.preferredWidth` logic for `centerColumn` and `rightPaneLoader` to reflect that `landingPage` no longer shows the right pane.

## Verification Plan

### Manual Verification
- **Home**: Verify the right-side task progress pane is completely hidden.
- **Other Pages**: Verify the right-side pane still appears as expected on pages like Corpse Cleaner or Task Selection.
