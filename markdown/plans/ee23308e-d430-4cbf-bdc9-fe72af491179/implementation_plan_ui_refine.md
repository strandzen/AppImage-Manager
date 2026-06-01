# Plan: Refine Storage Overview Bar Aesthetics

The user wants the Storage Overview Bar to look more native by having rounded corners that match the rest of the application. Currently, the bar has a fixed capsule radius (`height / 2`), and the inner colored segments appear sharp.

## Proposed Changes

### [UI Components]
#### [MODIFY] [StorageOverviewBar.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/StorageOverviewBar.qml)
- Update the `barFrame` Rectangle to use `SettingsManager.cornerRadius` for its `radius` property.
- To ensure the inner colored segments (the `Row` content) are correctly clipped to the rounded corners, enable `layer.enabled: true` on the `barFrame`.
- This will ensure that even the segments at the very ends of the bar appear rounded.

## Verification Plan

### Automated Tests
- None.

### Manual Verification
1. Run the application: `python main.py`.
2. Navigate to a page showing the Storage Overview Bar (likely the Home/Landing page).
3. Verify that the bar and its colored segments have rounded corners consistent with other UI panels.
4. (Optional) Change the "Corner Radius" in Settings and verify the bar updates accordingly.
