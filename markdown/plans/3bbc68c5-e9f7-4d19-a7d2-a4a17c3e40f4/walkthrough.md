# AppStream Description Extraction Implemented

The AppStream description extraction has been fully integrated into the AppImage Manager!

## What Changed?

### 1. Robust Offline Parsing (C++)
The `AppImageReader` class now actively looks for `.appdata.xml` or `.metainfo.xml` files inside the AppImage's SquashFS whenever it extracts metadata.
Using `QXmlStreamReader`, it safely parses the XML without the heavy overhead of a DOM tree. It specifically captures the `<description>` blocks, formats paragraphs (`<p>`) with double newlines, and bullet points (`<li>`) cleanly into plain text.

### 2. Automatic Fallback
If the developer of the AppImage did not include an AppStream file, the engine automatically falls back to the `Comment=` line from the `.desktop` file. This guarantees that you'll always have *something* to show.

### 3. Native UI Polish (QML)
The Dashboard's right pane has been reorganized:
- The old short comment label above the chips has been removed.
- A new scrollable `TextArea` (styled cleanly as a `Controls.Label`) has been added **directly below the version, size, and category chips**.
- It supports `Text.RichText` formatting (and links, if parsed), wraps cleanly, and handles long descriptions without breaking the layout.

## Verification
The project compiled perfectly with `cmake --build --preset dev`. 

You can run `appimagemanager` locally right now and select any major AppImage to see the new layout in action!
