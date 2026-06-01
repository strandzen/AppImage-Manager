# Task: Fix Font Reversion and Clean Up Debug Messages

- [x] Fix font reversion logic <!-- id: 0 -->
    - [x] Update `SettingsManager` to expose system default font properties <!-- id: 1 -->
    - [x] Populate system default font properties in `main.py` <!-- id: 2 -->
    - [x] Update QML to use explicit system defaults for font fallback <!-- id: 3 -->
- [x] Remove debug print from `main.py` <!-- id: 4 -->
- [x] Verify changes <!-- id: 5 -->

- [x] Refine Process List Layout Spacing <!-- id: 6 -->
    - [x] Increase right margin of process list rows and header <!-- id: 7 -->
    - [x] Verify horizontal alignment and breathing room <!-- id: 8 -->

- [x] Fix QML Warnings and Binding Loops <!-- id: 9 -->
    - [x] Explain "Binding Loops" and "Connection Warnings" to the user <!-- id: 10 -->
    - [x] Fix `idealWidth` binding loop in `CorpseCleanerPane.qml` <!-- id: 11 -->
    - [x] Fix model connection warnings in `TaskProgressPane.qml` <!-- id: 12 -->
    - [x] Fix `OverlaySheet` binding loops in `main.qml` and `SettingsPage.qml` <!-- id: 13 -->
    - [x] Fix `idealWidth` loop in `CorpseCleanerPane.qml` (via `main.qml` usage) <!-- id: 14 -->
    - [x] Fix recursive rearrange in `SystemMonitorPage.qml` header <!-- id: 15 -->
    - [x] Resolve remaining anchor and graphical object warnings <!-- id: 16 -->

- [x] Final QML Stabilization <!-- id: 17 -->
    - [x] Replace `background: null` with transparent Rectangles in all pages <!-- id: 18 -->
    - [x] Decouple `OverlaySheet` width from root container in `main.qml` <!-- id: 19 -->
    - [x] Stabilize `SettingsPage.qml` sheet content sizes <!-- id: 20 -->
    - [x] Final QML Stabilization <!-- id: 17 -->
    - [x] Replace `background: null` with transparent Rectangles in all pages <!-- id: 18 -->
    - [x] Decouple `OverlaySheet` width from root container in `main.qml` <!-- id: 19 -->
    - [x] Stabilize `SettingsPage.qml` sheet content sizes <!-- id: 20 -->
    - [x] Final verification of clean terminal output <!-- id: 21 -->

- [x] Phase 2: Eliminating Persistent Warnings <!-- id: 22 -->
    - [x] Fix redundant `width`/`height` bindings in `PackageManagerPane.qml` OverlaySheet <!-- id: 23 -->
    - [x] Simplify `OverlaySheet` sizing in `main.qml` and `SettingsPage.qml` <!-- id: 24 -->
    - [x] Revert `background` overrides to Kirigami defaults to fix scene warnings <!-- id: 25 -->
    - [ ] Add icon fallback for 404 AppImage icons <!-- id: 26 -->

- [x] Phase 3: Forced Stabilization <!-- id: 27 -->
    - [x] Delay initial page load in `main.qml` using `onCompleted` <!-- id: 28 -->
    - [x] Strictly decouple `OverlaySheet` widths from parents in `main.qml`, `PackageManagerPane.qml`, and `SettingsPage.qml` <!-- id: 29 -->
    - [x] Final verification and cleanup <!-- id: 30 -->

- [x] Phase 4: Final Bug Fixes & GitHub Push <!-- id: 31 -->
    - [x] Resolve anchor conflict in `main.qml` `globalConfirmSheet` <!-- id: 32 -->
    - [x] Delay `LandingPage` push with a Timer for scene-graph safety <!-- id: 33 -->
    - [x] Fix redundant anchors in `PackageManagerPane.qml` and `SettingsPage.qml` <!-- id: 34 -->
    - [x] Final verification run <!-- id: 35 -->
    - [x] Stage and commit all changes <!-- id: 36 -->
    - [x] Push to master <!-- id: 37 -->

- [/] Phase 5: UI Refinement - Conditional Separators <!-- id: 41 -->
    - [x] Hide separators in `SystemMonitorPage.qml` when alternating colors active <!-- id: 42 -->
    - [x] Hide separators in `PackageManagerPane.qml` when alternating colors active <!-- id: 43 -->
    - [x] Hide separators in `CorpseCleanerPane.qml` when alternating colors active <!-- id: 44 -->
    - [x] Hide separators in `TaskProgressPane.qml` when alternating colors active <!-- id: 45 -->
    - [x] Hide separators in `AppImageManagerPane.qml` when alternating colors active <!-- id: 46 -->
    - [x] Final verification and secondary push <!-- id: 47 -->

- [x] Phase 6: Fix Process List Clipping <!-- id: 53 -->
    - [x] Add vertical separator and right padding to `appList` in `SystemMonitorPage.qml` <!-- id: 54 -->
    - [x] Update `ItemDelegate` width and row margins <!-- id: 55 -->
    - [x] Align `listHeader` margins <!-- id: 56 -->
    - [x] Verify fix locally <!-- id: 57 -->
