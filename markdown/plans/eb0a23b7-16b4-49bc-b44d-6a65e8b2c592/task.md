# Grand Masterplan Execution Checklist

## ⚡ Phase 1: Asynchronous Core & Native Parsing
- [x] **Asynchronous Theme Generation**
  - [x] Implement `QThread` or `QtConcurrent` in `ComposerEngine` to copy files & build packs in a background worker thread.
  - [x] Expose progress signals from C++ to QML.
- [x] **UI Loading Indicators**
  - [x] Add a beautiful circular loading indicator in `PlasmaMixerPage.qml` during async operations.
  - [x] Disable generate buttons/fields while building is in progress to prevent overlaps.

## 💾 Phase 2: Local JSON Presets (Save/Load/Delete)
- [x] **Preset Storage Engine (C++)**
  - [x] Implement profile load/save methods using `QJsonDocument` to manage configurations inside `~/.config/plasmathemecomposer/presets.json`.
- [x] **Preset UI Cards**
  - [x] Add a visual "Saved Presets" selector card in the Mixer page to easily name, apply, or delete custom mixtures.

## 🎨 Phase 3: Wallpaper Picker & Editable Text Previews
- [x] **Custom Wallpaper Selection**
  - [x] Add a filesystem image file selector to load custom wallpaper backgrounds into the Live Preview card.
- [x] **Editable Preview Text Fields**
  - [x] Evolve the static preview dialog & notification texts into editable controls (type directly into the preview!).

## 📦 Phase 4: Desktop Integration & XDG Compliance
- [x] **Launcher & Packaging**
  - [x] Add a native launcher `.desktop` file, scalable icons, and Appstream metadata for professional Discover package registration.
