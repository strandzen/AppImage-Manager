# AppStream Description Task List

- `[x]` Core Backend (`appimageinfo.h` & `appimagereader.cpp`)
  - `[x]` Add `description` field to `AppImageInfo`
  - `[x]` Implement `findAppStreamFile()`
  - `[x]` Implement `extractDescription()` with `QXmlStreamReader`
  - `[x]` Update `readWithLibappimage()` to use it with a fallback
- `[x]` GUI Backend (`appimagelistmodel.cpp`)
  - `[x]` Add `DescriptionRole` to enum and role names
  - `[x]` Expose `description` in `data()`
- `[x]` Frontend (`DashboardWindow.qml`)
  - `[x]` Add `description` to `currentItem` snapshot
  - `[x]` Remove old comment label
  - `[x]` Add new description Label below the chips row
- `[x]` Verification
  - `[x]` Compile and run tests
