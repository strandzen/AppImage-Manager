# Drag-to-Reorder Store Sources & Source Priority Installation Choice

This plan outlines how we will implement drag-to-reorder for AppImage sources in the Settings page, and how that order determines the preferred installation method (direct download vs. AM script) when an application exists in multiple sources.

## User Review Required

> [!IMPORTANT]
> The source order is persisted in `QSettings` under `Store/sourcesJson` by saving the list of sources in the user's preferred order. The top-most enabled source in the list that contains a package will dictate the default installation action:
> - If the top-most source is of type `am-database`, or is a `universal` source where the app has `hasAmScript = true`, the primary button displays **"Install via AM"**.
> - Otherwise (e.g. `appimagehub` or a `universal` source with `downloadUrl`), the primary button displays **"Download"**.

> [!TIP]
> To ensure the drag-to-reorder action is fluid and doesn't get interrupted by background threads or model resets mid-drag, the reordering will happen locally within a QML `ListModel`. The new order is sent to the C++ backend and saved to disk only when the user releases the drag handle.

## Proposed Changes

### C++ Backend (`src/gui/`)

#### [MODIFY] [amstoremodel.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.h)
- Declare the new invokable method:
  ```cpp
  Q_INVOKABLE void setSourcesOrder(const QStringList &idOrder);
  ```

#### [MODIFY] [amstoremodel.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.cpp)
- Implement `setSourcesOrder(const QStringList &idOrder)`:
  - Reorder the items in `m_sources` to match the given `idOrder`.
  - Save the updated sources using `saveSources()`.
  - Trigger a metadata reload by calling `checkAllLoaded()`.
- Update duplicate merging in `checkAllLoaded()` to be source-agnostic:
  - If a source of any type has AM scripts (`source.type == "am-database"` or `app.hasAmScript`), mark `existing.hasAmScript = true`.
  - If a source of any type has feed/download data (`source.type == "appimagehub"` or `!app.downloadUrl.isEmpty()`), mark `existing.hasFeedData = true` and merge metadata fields.
- Update `app.preferAm` selection in `checkAllLoaded()`:
  - Iterate through `sourcesData` (which matches the user-defined order) to find the first source containing the app.
  - Set `app.preferAm` to `true` if that source is `am-database` or if the app's parsed record in that source has `hasAmScript = true`.

---

### QML UI Components (`qml/`)

#### [MODIFY] [SettingsPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsPage.qml)
- Add a local QML `ListModel` named `localSourcesModel`.
- Implement `syncLocalModel()` to populate `localSourcesModel` from `amStoreModel.sources`.
- Wire `syncLocalModel()` to run on page `Component.onCompleted` and inside a `Connections` target for `amStoreModel` on `onSourcesChanged` (ignoring updates if the user is actively dragging).
- Update the `ListView` to use `model: localSourcesModel`.
- Update delegate role references from `modelData.xxx` to `model.xxx` (e.g. `model.name`, `model.url`).
- Update the drag handle's `MouseArea` to:
  - Keep track of the active drag state.
  - On `onReleased`, collect the IDs in the new order and call `amStoreModel.setSourcesOrder(ids)`.
- Configure `move` and `displaced` transitions on `sourcesListView` to animate shifting items smoothly.

---

## Verification Plan

### Automated Tests
- Run existing unit tests to confirm no regressions:
  ```bash
  ctest --test-dir build/dev --output-on-failure
  ```

### Manual Verification
- Open Settings and verify the list of sources under **AppImage Sources**.
- Drag the grab handle of **AppImage Hub** to the top and release.
  - Go to Discover, find an app present in both (e.g. `Kdenlive` or `Audacity`).
  - Verify that the primary install button displays **"Download"** (direct download).
- Drag the grab handle of **AM Database** to the top and release.
  - Go to Discover, check the same app.
  - Verify that the primary install button displays **"Install via AM"** (recipe script).
- Add a custom `universal` JSON source, and verify that dragging it around alters priority correctly.
