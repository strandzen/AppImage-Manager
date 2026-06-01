# Senior KDE Plasma Dev Review — UI/UX Improvement Plan

## Context
Constructive review of Plasma Theme Composer from the perspective of a Senior KDE Plasma developer. Philosophy: ship the simplest thing that works. Every extra control is a support burden. Prioritized by user impact.

---

## The Core Problem

The app models the domain incorrectly. Plasma themes don't "mix" — they **inherit**. Every theme falls back to `default` for missing files. So when the user picks "Dialogs: Breeze Dark" they're not getting Breeze Dark dialogs — they're getting Breeze Dark's `dialogs/background.svgz` if it exists, otherwise `default`'s. The UI implies more control than actually exists, and gives no feedback when a selection has no effect.

---

## Critical Fixes (do these first)

### 1. Delete `GeneratePage.qml`

**Why:** It does nothing. The user just came from a page where they selected everything. Showing a summary they already know adds friction without value. We already moved the Generate button inline — finish the job by removing the page entirely and inlining the theme name field on `PlasmaMixerPage`.

**What to do:**
- Add a `TextField` for theme name to the right-column controls (top or bottom)
- The `Generate` button already exists inline — wire it directly to `ComposerEngine`
- Delete `GeneratePage.qml`
- Remove the `pageStack.push(GeneratePage)` call

**Impact:** Removes an entire dead page, halves the number of steps, eliminates the confusing "Next: Generate" detour.

---

### 2. Show "No override — inherits default" in preview instead of hiding elements

**Why:** Right now, if a theme has no `dialogs/background.svgz`, the dialog box in the preview just disappears silently. The user thinks the preview is broken. In reality, at runtime KDE would show the `default` theme's dialog — the preview is lying by omission.

**What to do:**
- When `resolvePreviewPath()` returns `""`, show the element anyway using the **`default` theme path** as the fallback: `ComposerEngine.resolveSvgPath("/usr/share/plasma/desktoptheme/default/" + subPath)`
- Add a subtle italic label beneath the preview when any element is using the fallback: "Some elements inherit from default"

**Impact:** Preview becomes truthful. User understands what they're actually getting.

---

### 3. Add "Apply to Desktop" button after generation

**Why:** After generating, the user still has to open System Settings → Appearance → Plasma Style and select their new theme manually. This is the last mile that makes the tool feel incomplete.

**What to do:**
- Add `Q_INVOKABLE bool applyPlasmaTheme(const QString &themeName)` to `ComposerEngine.cpp`
- Implementation: `QProcess::startDetached("plasma-apply-desktoptheme", {themeName})`
- Show it as a secondary button after successful generation: "Apply to Desktop"
- Keep it optional — not every user wants to apply immediately

**Impact:** Makes the tool actually useful end-to-end without leaving the app.

---

## Medium Priority

### 4. Rename "Opaque Elements" / "Translucent Elements" — or hide them

**Why:** Nobody knows what these mean without reading KDE compositor documentation. "Opaque" in Plasma means "used when compositor is OFF (no blur/transparency)". "Translucent" means "used when compositor is ON". The labels are KDE-internal terminology, not user vocabulary.

**What to do:**
- Wrap both combos inside a `Kirigami.ExpandableListItem` (or a `CheckBox`-gated reveal) under the Advanced section separator
- Default: collapsed
- Header label: "Compositor Overrides" with description "Override SVGs when compositor is on or off"
- Rename internally: "Without Compositor (Opaque)" / "With Compositor (Translucent)" so the label is self-explanatory when expanded

---

### 5. Replace color swatches with a role-labelled palette

**Why:** 10 unlabelled circles of color tell the user nothing. A theme's blue accent circle looks the same as its background circle at 24px. What the user actually cares about: "what will my window backgrounds look like? What color is text?"

**What to do:**
- Replace the 10 anonymous swatches with 5 labelled pairs (background, text, highlight, button, link)
- Extract specific color group/key pairs from `KConfig` in `extractThemeColors()` rather than deduplicating all hex values
- Show them as small labeled chips: `[  ] Window BG   [  ] Text   [  ] Highlight`

**Impact:** Color section becomes informative rather than decorative.

---

### 6. Warn before overwriting an existing theme

**Why:** `ComposerEngine::generatePlasmaTheme()` calls `dir.removeRecursively()` with no confirmation if the theme name already exists. A user who hand-edited their generated theme and regenerates will silently lose their edits.

**What to do:**
- Add `Q_INVOKABLE bool themeExists(const QString &name)` to ComposerEngine
- Check in QML before calling generate: if `themeExists(name)` show a `Kirigami.PromptDialog` "Theme 'X' already exists. Overwrite?"
- One dialog, one `onAccepted` call. No backend changes needed.

---

## Low Priority / Polish

### 7. Remove `PreviewPage.qml`

Dead code. Never reachable from the page stack. Delete it.

---

### 8. Base Theme combo should show current selection state

**Why:** After picking a Base Theme and then overriding individual components, the Base Theme combo still shows the last-picked base. It looks like all components are still from that base when they're not. This is misleading.

**What to do:**
- When any individual component combo diverges from the base theme, change the Base Theme combo display text to "Custom"
- Simplest implementation: a computed property `property bool isCustomMix` that checks if all combos match `baseThemeCombo.currentIndex`; if not, show placeholder text "Custom mix"

---

## What NOT to Do

- **Do not add drag-and-drop.** The combo box model is correct for this domain. D&D would add touch/accessibility complexity for no real gain.
- **Do not add undo/redo.** State is cheap to re-configure. The right answer is: make the UI fast to use, not reversible.
- **Do not add save/load of draft configurations.** For a utility this size, users run it, make a theme, close it. Persisting draft state adds serialization complexity. If needed later, KConfig can handle it in 20 lines.
- **Do not replace the two-column layout.** It's correct. Preview left, controls right is the right model for a configuration utility.

---

## Prioritized Implementation Order

| # | Change | Files | Effort |
|---|--------|-------|--------|
| 1 | Delete GeneratePage, inline name field + generate | PlasmaMixerPage.qml, GeneratePage.qml, main.qml | S |
| 2 | Fallback preview to `default` theme assets | PlasmaMixerPage.qml, resolvePreviewPath() | S |
| 3 | Add `applyPlasmaTheme()` + Apply button | ComposerEngine.cpp/.h, PlasmaMixerPage.qml | S |
| 4 | Rename/collapse opaque+translucent controls | PlasmaMixerPage.qml | XS |
| 5 | Role-labelled color palette | ComposerEngine.cpp (extractThemeColors), PlasmaMixerPage.qml | M |
| 6 | Overwrite confirmation dialog | PlasmaMixerPage.qml, ComposerEngine.h/.cpp | S |
| 7 | Delete PreviewPage.qml | PreviewPage.qml | XS |
| 8 | "Custom mix" base theme indicator | PlasmaMixerPage.qml | XS |

## Verification
Each change is self-contained and verifiable by running `./build/bin/plasma-theme-composer` and exercising the affected flow.
