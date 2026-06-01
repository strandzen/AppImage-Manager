# Goal Description
The purpose of this fix is to resolve three visual bugs in the Corpse Cleaner's "Confirm Deletion" popup:
1. The orange text color does not match the user's "emphasis color" setting.
2. The popup dialog does not scale properly to fit its contents (clipping occurs).
3. The "Clean Now" button displays text twice with different colors.

## Proposed Changes

### UI Updates (`qml/main.qml`)

#### Fix Orange Text Color (Emphasis color)
- **Modify** the `Label` at the top of `globalConfirmSheet` that displays the warning text ("The following specific folders will be completely deleted..."):
  - Change `color: Kirigami.Theme.neutralTextColor` to dynamically use `SettingsManager.emphasisColor !== "" ? SettingsManager.emphasisColor : Kirigami.Theme.highlightColor` to match the rest of the application's emphasis.

#### Fix Popup Scaling/Clipping
- **Modify** the sizing properties inside `globalConfirmSheet`:
  - The `ColumnLayout` with `id: confirmLayout` currently has a hardcoded string `implicitHeight: Kirigami.Units.gridUnit * 12 // Pin height to break loop`. This restricts the entire column. Remove or replace it with flexible anchoring or a larger maximum bound.
  - The `Rectangle` surrounding the `ListView` has `Layout.preferredHeight: Kirigami.Units.gridUnit * 12`. This should be permitted to shrink or grow, but we'll ensure `Layout.minimumHeight` and `Layout.maximumHeight` (or similar) are set so it can fit properly.
  - The `globalConfirmSheet` itself can be allowed to dictate standard sizing.

#### Fix "Clean Now" Button Text Duplication
- **Modify** the "Clean Now" `Button` with `id: globalCleanBtn`. 
  - The button has `text: "Clean Now"` set, but also defines a custom `contentItem` containing another `Label` with the text. This causes Kirigami/Qt to overlay the default text rendering with the custom one.
  - Set `display: AbstractButton.IconOnly` or just remove the top-level `text: "Clean Now"` attribute and rely solely on the interior text representation, or vice versa, ensuring the red styling (`Kirigami.Theme.negativeTextColor`) remains.

### Action Buttons Refactor

We will create a unified `ActionButton` component and use it across the application for consistently styled action buttons. This will align button height and coloring.

#### [NEW] `qml/components/ActionButton.qml`
- Create a reusable `ActionButton` component extending `QtQuick.Controls.Button`.
- Establish standard height `Kirigami.Units.gridUnit * 1.8`.
- Add `actionCategory` property with values `"system"`, `"deletion"`, and `"running"`.
- Set the background and text colors based on the `actionCategory` and new properties in `UIColorsManager`.

#### [MODIFY] `models/ui_colors_manager.py`
- Add new keys to `_DEFAULTS`:
    - `button_system_hex`: Hex color for standard buttons (defaults to Kirigami.Theme.highlightColor)
    - `button_deletion_hex`: Hex color for deletion buttons (red)
    - `button_running_hex`: Hex color for running buttons (green)

#### [MODIFY] Button references in QML files
- Replace explicit `QtQuick.Controls.Button` styling with the new `ActionButton` in:
    - `qml/components/PackageManagerPane.qml`
    - `qml/components/AppImageManagerPane.qml`
    - `qml/components/TaskProgressPane.qml`
    - `qml/main.qml` (corpse cleaner global confirm sheet)

### Full Colored Action Buttons Toggle
#### [MODIFY] models/settings_manager.py
- Add a new pyqtProperty `fullColoredButtons` matching the standard structure. Default to `False`. Emits `fullColoredButtonsChanged`.

#### [MODIFY] qml/pages/SettingsPage.qml
- Add a Kirigami `Switch` component in the UI section, mapping to `SettingsManager.fullColoredButtons`. Label it "Full Colored Action Buttons". 

#### [MODIFY] qml/components/ActionButton.qml
- Adjust the internal property logic. If `SettingsManager.fullColoredButtons` is checked:
  - Fill the `background` rectangle completely with `baseColor` natively. Apply shading (using `Qt.darker` or `Qt.lighter`) for `hovered` and `down` states.
  - Set the active text and icon colors to `Qt.darker(baseColor, 2.5)` (to ensure the text is a dark variant of the background). If disabled, render with the theme disabled fallback.

## Verification Plan

### Manual Verification
1. Open the application.
2. If the user's "Emphasis Color" is set (e.g. pink), verify that opening the Corpse Cleaner confirmation displays the warning text in pink.
3. Add/mock fake corpses to fill up the dialog, or generate generic items to verify that the popup dialog scales its height properly without cutting off the bottom row of buttons or the total free space text.
4. Check the "Clean Now" button on the confirmation popup to ensure the text only appears once, in red, and is properly aligned with the trashcan icon.
