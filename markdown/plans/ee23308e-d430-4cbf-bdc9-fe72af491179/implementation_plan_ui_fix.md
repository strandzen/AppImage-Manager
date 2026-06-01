# Plan: Fix Storage Bar Rendering and Aesthetics

The previous attempt to apply rounded corners using `layer.effect` failed because it effectively replaced the rendering of the bar's internal segments. Additionally, standard `Rectangle` clipping in Qt Quick does not anti-alias rounded corners perfectly.

## Proposed Changes

### [UI Components]
#### [MODIFY] [StorageOverviewBar.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/StorageOverviewBar.qml)
- **Revert `layer.effect`**: Remove the broken `layer.effect` and `layer.enabled` properties from `barFrame`.
- **Use `Kirigami.ShadowedRectangle` for segments**: Replace the internal `Rectangle` segments with `Kirigami.ShadowedRectangle`.
- **Apply Selective Rounding**: 
    - For the first segment (Packages), apply `corners.topLeftRadius` and `corners.bottomLeftRadius` using `SettingsManager.cornerRadius`.
    - For the last segment (System), apply `corners.topRightRadius` and `corners.bottomRightRadius` using `SettingsManager.cornerRadius`.
    - If a segment is both first and last (only one category has data), apply rounding to all corners.
- This approach provides perfectly anti-aliased rounded corners without relying on complex clipping layers.

## Verification Plan

### Manual Verification
1. Run `python main.py`.
2. Verify the Storage Bar is visible again.
3. Confirm that the far-left and far-right edges of the bar are correctly rounded.
4. Verify that intermediate segments remain sharp where they touch other segments.
