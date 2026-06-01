# Plan: Final Fix for Storage Bar Rounding

The previous clipping-based approach failed because the inner bar segments were not correctly rounded at the parent's edges. We will now implement per-segment corner rounding.

## Proposed Changes

### [UI Components]
#### [MODIFY] [StorageOverviewBar.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/StorageOverviewBar.qml)
- **Use `Kirigami.ShadowedRectangle` for segments**: Replace all internal `Rectangle` items in the `Row` with `Kirigami.ShadowedRectangle`.
- **Implement Visibility Logic**: 
    - Create helper functions `isFirstVisible(type)` and `isLastVisible(type)` to determine if a specific data segment is at the start or end of the current bar display.
- **Apply Specific Radii**:
    - Each segment will have its `corners.topLeftRadius` and `corners.bottomLeftRadius` set to `SettingsManager.cornerRadius` ONLY if it is the first visible segment.
    - Each segment will have its `corners.topRightRadius` and `corners.bottomRightRadius` set to `SettingsManager.cornerRadius` ONLY if it is the last visible segment.
- This ensures that no matter which categories have data, the bar always starts and ends with a smooth rounded corner.

## Verification Plan

### Manual Verification
1. Run `python main.py`.
2. Observe the Storage Bar with various data states (e.g. only Packages, only System, or a mix).
3. Verify that the outer ends are ALWAYS rounded according to the Settings.
4. Verify that there are no "gaps" or "pill shapes" between adjacent segments.
