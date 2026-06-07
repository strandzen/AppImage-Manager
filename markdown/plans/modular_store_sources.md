# Plan: Modular Discover Sources for AppImage Manager

This plan details the design and implementation to decouple the Discover store catalog from hardcoded databases. We will make the sources dynamic, configurable by the user, and persisted in `QSettings`. This will allow the user to modify, remove, or add new sources (of both JSON feed and AM text database types) and choose to avoid dependency on `am`/`appman` entirely by disabling or deleting the AM Database source.

## Proposed Changes

We will introduce a `StoreSource` struct to represent a dynamic AppImage source. Sources will be serialized/deserialized to/from `QSettings` as a JSON array under the key `Store/sourcesJson`. The default sources list will include **AppImage Hub** and **AM Database** to match current behavior.

### C++ Backend (`src/gui/`)

#### [MODIFY] [amstoremodel.h](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.h)
- Declare `StoreSource` struct.
- Expose `sources` property.
- Add `sourcesChanged` signal.
- Add `sourceIds` field to `StoreApp`.
- Declare invokable methods for managing sources (`addSource`, `updateSource`, `removeSource`, `resetSourcesToDefault`). These will perform strict URL validation using `QUrl(url).isValid()`.

#### [MODIFY] [amstoremodel.cpp](file:///home/herman/Documents/Project/AppImageManager/src/gui/amstoremodel.cpp)
- Implement `loadSources()` and `saveSources()`.
- Update `load()` to handle any number of active network requests and parser threads dynamically.
- Update `applyFilter()` and `updateAvailableCategories()` to filter based on the active source's ID.

### QML UI Components (`qml/`)

#### [MODIFY] [StorePage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/StorePage.qml)
- Populate the source filter menu dynamically using `Instantiator` over `amStoreModel.sources`.

#### [MODIFY] [SettingsPage.qml](file:///home/herman/Documents/Project/AppImageManager/qml/SettingsPage.qml)
- Add "AppImage Sources" FormCard.
- Implement inline add/edit `sourceDialog` (Kirigami.Dialog).
