# AppStream XML Description Fix

The description from the AppStream XML was not appearing in the dashboard's detail pane due to two issues in the C++ extraction backend:

1. **Path Resolution for XML Files:** Many AppImages store their root files with a `./` prefix internally. The `findAppStreamFile()` function in `AppImageReader` was looking strictly for paths starting with `usr/share/metainfo/` or `usr/share/appdata/`, causing it to completely miss the XML files inside these AppImages.
2. **RichText Line Breaks:** The dashboard's detail pane renders the extracted description using QML's `Text.RichText` to allow clickable hyperlinks. The XML parser was emitting plain `\n` characters for line breaks. In HTML/RichText, `\n` is interpreted simply as a space. This caused any extracted description to either render improperly on one single line or glitch completely if it contained unescaped HTML characters.

### Changes Made
- Modified `AppImageReader::findAppStreamFile` to properly strip any leading `./` before performing path matching, allowing it to correctly locate and extract the metadata XML.
- Updated `AppImageReader::extractDescriptionFromXml` to emit `<br><br>` for `<p>` tags and `<br>• ` for `<li>` list elements.
- Implemented HTML escaping in `extractDescriptionFromXml` (`&amp;`, `&lt;`, `&gt;`) to ensure any characters in the description string do not break the QML `RichText` parser.

These changes ensure that rich application descriptions will accurately reflect within the right-side detail pane whenever an AppImage is selected in the dashboard.
