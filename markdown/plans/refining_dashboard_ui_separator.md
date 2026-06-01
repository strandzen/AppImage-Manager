# Implementation Plan: AppStream XML Descriptions

We will implement the native AppStream XML extraction approach, falling back to the standard `.desktop` comment if the XML is not present. This ensures no network overhead while providing rich descriptions.

## Proposed Changes

### Core Component (`appimagemanager_core`)

#### [MODIFY] `src/core/appimageinfo.h`
- Add a new field `QString description;` to the `AppImageInfo` struct to hold the extracted description paragraph.

#### [MODIFY] `src/core/appimagereader.h` & `src/core/appimagereader.cpp`
- **New Method `findAppStreamFile()`**: Scans the AppImage directory tree for files matching `usr/share/metainfo/*.appdata.xml` or `usr/share/appdata/*.xml`.
- **New Method `extractDescription(QByteArray xml)`**: Uses `QXmlStreamReader` to quickly and safely parse the XML buffer, looking for the `<description>` tag (specifically `<p>` paragraphs within it), and joins them into a clean string, stripping out raw HTML tags.
- **Update `readWithLibappimage()`**: Call the above methods during the metadata extraction phase. If the parsed description is empty or missing, it will gracefully fallback to the `Comment=` value parsed from the `.desktop` file.

---

### GUI Component (`appimagemanager_qml`)

#### [MODIFY] `src/gui/appimagelistmodel.h` & `src/gui/appimagelistmodel.cpp`
- **New Role**: Add `DescriptionRole` to the `AppImageListModel::Roles` enum.
- **Role Mapping**: Map `"description"` to `DescriptionRole` in `roleNames()`.
- **Data Fetching**: Return `item.info.description` in the `data()` method.

---

### User Interface

#### [MODIFY] `qml/DashboardWindow.qml`
- **Snapshot Update**: Update `refreshCurrentItemAt(idx)` to pull the new `description` property from the model into `root.currentItem`.
- **UI Layout**: 
  - Remove the current `comment` label that sits *above* the chips row.
  - Add a new `Controls.Label` (or a scrollable `TextArea` if it's long) **below** the `chipsRow` (Version, Size, Category).
  - Bind the text to `root.currentItem.description`.
  - Format it with `wrapMode: Text.WordWrap`, `textFormat: Text.RichText` (or PlainText depending on extraction), and appropriate opacity for readability.

## Verification Plan
1. Compile the project with `cmake --build --preset dev`.
2. Launch the application.
3. Select an AppImage known to have AppStream metadata (e.g., Krita, OBS Studio).
4. Verify that a multi-sentence description appears perfectly under the version/size chips in the right pane.
5. Select a simpler AppImage without AppStream data and verify it falls back to the short 1-sentence comment gracefully.
