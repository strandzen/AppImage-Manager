# Plan: Refactor System Monitor UI

Align the System Monitor page with the application's modern design language and the "AppImages" aesthetic.

## Proposed Changes

### [UI Components]
#### [MODIFY] [SystemMonitorPage.qml](file:///home/herman/Documents/Project/Maintainer/qml/pages/SystemMonitorPage.qml)
- **Replace Storage Bar**: Swap the custom `ProgressBar` implementation for `MyComponents.StorageOverviewBar`.
- **Top-align Metrics**:
    - Reorder elements in the "Overview" tab so that the CPU, Memory, and Network metrics are the very first items.
    - Reduce vertical spacing and remove redundant nested margins to minimize "empty space".
- **Fix Margins**:
    - Adjust the `ColumnLayout` anchors and margins to match the padding observed in `AppImageManagerPane.qml`.
    - Remove `anchors.fill: parent` from nested layouts (which causes double margins) and use `Layout.fillWidth: true`.
- **Expand Tabs**:
    - Modify the `TabBar` inside Monitor to use `width: parent.width / 2` (or similar) for its buttons so they fill the horizontal space.

#### [MODIFY] [AppImageManagerPane.qml](file:///home/herman/Documents/Project/Maintainer/qml/components/AppImageManagerPane.qml) (Optional/Contextual)
- Verify if any margin adjustments are needed here to ensure a perfect match, though this serves as the "source of truth" for the desired look.

## Verification Plan

### Manual Verification
1. Run `python main.py`.
2. Navigate to the **Monitor** tab.
3. Observe the new Storage Bar with rounded segments.
4. Verify that CPU/Memory/Net metrics are at the top and the huge gap is gone.
5. Check that the "Overview" and "Processes" tabs stretch to fill the width.
6. Toggle between **Monitor** and **AppImages** to ensure margins and border aesthetics are identical.
