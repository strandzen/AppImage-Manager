# Improve Chip Contrast on Hover

The issue occurs because the `Kirigami.Chip` components in the `DashboardWindow.qml` list view use a color scheme that blends in with the `Kirigami.Theme.hoverColor` and `Kirigami.Theme.highlightColor` used by the row delegate's background.

## User Review Required
> [!IMPORTANT]
> Please review the proposed changes below. By default, I will apply this fix to the **Visible/Hidden** chip, but I can also apply it to the **Version** and **Size** chips to ensure uniform behavior across all columns. Let me know if you want all chips updated.

## Open Questions
> [!QUESTION]
> Do you have a specific KDE Kirigami color set in mind for the "accent" color when the row is highlighted?
> Some options are:
> - `Kirigami.Theme.Complementary` (usually a contrasting accent color)
> - `Kirigami.Theme.Button` (standard button color, often provides enough contrast)
> - `Kirigami.Theme.Selection` (might blend with the highlight, but sometimes inverted)
> I will use `Kirigami.Theme.Complementary` as the default approach unless you prefer another.

## Proposed Changes

### Dashboard Window UI

#### [MODIFY] [DashboardWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/DashboardWindow.qml)
- Locate the "Visible-in-Search column" and its `Kirigami.Chip` component.
- Add dynamic styling based on the row's state:
  ```qml
  Kirigami.Theme.colorSet: (delegate.hovered || delegate.highlighted) 
                           ? Kirigami.Theme.Complementary 
                           : Kirigami.Theme.Window
  ```
- (Optional) Apply similar logic to the "Size column" chip and "Version column" chip to maintain visual consistency across the row when hovered.

## Verification Plan

### Manual Verification
1. Open the AppImage Manager Dashboard.
2. Hover the mouse over an installed AppImage entry.
3. Verify that the "Visible" / "Hidden" chip changes its color to a contrasting accent color, remaining clearly visible against the highlighted row background.
4. Verify the regular state (not hovered) remains unaffected.
