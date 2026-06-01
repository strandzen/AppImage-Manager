# UI Polish Plan — Plasma Theme Composer

## Context

All prior improvements from the previous plan are complete (two-column layout, GeneratePage summary, title fixes, swatches, window min size). This plan addresses the next layer: native KDE polish, removing dead UI, fixing visual inconsistencies, and small functional bugs. No new features.

---

## Part 1 — Visual

### V1 · Dark/Light toggle as preview overlay button
**Problem:** `RowLayout { Label + Switch }` below the preview is a form-style control for a preview affordance. Wastes vertical space, looks out of place next to color swatches.

**Fix:** Remove the RowLayout (lines 385–393 PlasmaMixerPage). Add a `ToolButton` as a sibling to `PreviewViewport` inside `previewRoot` (z:2, not clipped by viewport):
```qml
ToolButton {
    anchors.top: parent.top; anchors.right: parent.right
    anchors.topMargin: 6; anchors.rightMargin: 6
    z: 2
    icon.name: useDarkBg ? "weather-clear-night" : "weather-clear"
    ToolTip.visible: hovered; ToolTip.delay: 500
    ToolTip.text: useDarkBg ? i18n("Switch to light preview") : i18n("Switch to dark preview")
    onClicked: useDarkBg = !useDarkBg
}
```

---

### V2 · Interactive hint in preview heading row
**Problem:** No affordance that taskbar/clock are interactive. Users will miss the interactivity entirely.

**Fix:** Replace the standalone heading with a `RowLayout` carrying a right-aligned caption:
```qml
RowLayout {
    Layout.fillWidth: true
    Kirigami.Heading { text: i18n("Live Preview"); level: 4; opacity: 0.65; Layout.fillWidth: true }
    Label {
        text: i18n("Hover taskbar · Click clock")
        font.pixelSize: 9; opacity: 0.4
        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
    }
}
```

---

### V3 · Color swatch border opacity
**Problem:** `border.color: Kirigami.Theme.textColor` — full-opacity border looks harsh against any swatch color.

**Fix:** One line in swatch `Rectangle`:
```qml
border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.35)
```

---

### V4 · GlobalDrawer: isMenu true
**Problem:** `isMenu: false` opens a full sidebar drawer for a single "About" action. Massively overbuilt.

**Fix:** `main.qml` — `isMenu: false` → `isMenu: true`. Lightweight popup menu instead.

---

### V5 · "Advanced" label on Opaque/Translucent section
**Problem:** Opaque/Translucent combos have no visible preview effect, but sit visually identical to main component combos. Creates cognitive noise.

**Fix:** Change the existing `Kirigami.Separator` before them (PlasmaMixerPage line ~524) to carry a label:
```qml
Kirigami.Separator {
    Kirigami.FormData.isSection: true
    Kirigami.FormData.label: i18n("Advanced")
}
```

---

### V6 · Generate button: primary accent style
**Problem:** Plain `Button` on GeneratePage. Primary action on a terminal page should be visually distinguished.

**Fix:** Add `highlighted: true` to the Generate Button. Standard KDE pattern — renders with Breeze accent-color fill.

---

### V7 · WelcomePage cleanup
**Problem:** WelcomePage is opened via "About" but contains a "Start Mixing" button that pushes a second instance of PlasmaMixerPage (which is already the initialPage). Broken navigation stack, wrong mental model.

**Fix:**
- Remove the `Button { text: i18n("Start Mixing") }` block
- Add `Kirigami.Icon { source: "plasma"; implicitWidth: 64; implicitHeight: 64; Layout.alignment: Qt.AlignHCenter }`
- Add `Label { text: i18n("Version 1.0"); opacity: 0.55; Layout.alignment: Qt.AlignHCenter }`

---

## Part 2 — Functional

### F1 · Remove dead "Back" action from PlasmaMixerPage footer
**Problem:** PlasmaMixerPage is `pageStack.initialPage`. `pageStack.pop()` on the initial page is a no-op. The "Back" action is visible but does nothing.

**Fix:** Remove the Back `Kirigami.Action` from the PlasmaMixerPage footer. Only "Next: Generate" remains.

---

### F2 · Guard icon model role lookup against empty model
**Problem:** `model.data(model.index(currentIndex, 0), iconModel.IdRole)` — when `iconModel` has 0 items, `currentIndex` is -1. `model.index(-1, 0)` is invalid. Silent undefined behaviour.

**Fix:** In `appIconsCombo` and `trayIconsCombo`, both `onActivated` and `Component.onCompleted` (4 locations total):
```qml
currentIndex >= 0 ? model.data(model.index(currentIndex, 0), iconModel.IdRole) : ""
```

---

### F3 · Reset preview state on base theme change
**Problem:** User dismisses notification (✕) or hides calendar, then changes base theme. Preview stays in dismissed state — right column blank. No reset mechanism.

**Fix:** Add two lines to `applyBaseTheme()` in PlasmaMixerPage:
```qml
previewRoot.notificationShown = true
previewRoot.calendarShown = true
```

---

## Files Changed

| File | Changes |
|------|---------|
| `src/qml/pages/PlasmaMixerPage.qml` | V1, V2, V3, V5, F1, F2, F3 |
| `src/qml/pages/GeneratePage.qml` | V6 |
| `src/qml/pages/WelcomePage.qml` | V7 |
| `src/qml/main.qml` | V4 |

---

## Verification

1. `make -j$(nproc)` — clean build
2. Preview overlay toggle button visible top-right corner of preview
3. Hover taskbar → tooltip appears above hovered item; click clock → calendar toggles
4. Change base theme after dismissing notification → notification reappears
5. Global drawer "About" → lightweight popup menu, not sidebar
6. WelcomePage: app icon + version, no "Start Mixing" button
7. PlasmaMixerPage footer: only "Next: Generate"
8. GeneratePage "Generate" button has accent fill
