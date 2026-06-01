# Senior KDE Plasma Dev Review: PlasmaThemeEditor

## Context
Full audit of the app from a KDE contributor perspective — what's solid, what's broken, what's missing, and where performance is left on the table.

---

## What Works Well

- CMake + ECM setup is correct: proper `find_package(ECM)`, `qt_add_qml_module`, KDE install paths
- Desktop file + AppStream XML are present and structurally valid
- i18n infrastructure is wired (KLocalizedString domain, KLocalizedContext in engine, i18n() calls in QML)
- Models (`ThemeListModel`, `IconPackListModel`) follow proper `QAbstractListModel` pattern with role-based access
- Async theme generation via `QThread` is the right call — no UI freeze
- `ComposerEngine` API is clean: invokables are well-scoped, SVG path resolution handles .svgz/.svg fallback
- `resolveSvgPath` pattern enforced consistently in docs/code
- Kirigami 6 patterns are current — no deprecated API usage detected
- Preset save/load via JSON is functional and persistent

---

## Critical Bugs

### 1. Thread use-after-free (`ComposerEngine.cpp:234-256`)
Lambda in `QThread::create` captures `this` by value. The nested `QMetaObject::invokeMethod` lambda also captures `this`. If `ComposerEngine` is destroyed while worker runs → use-after-free crash.
**Fix:** Use `QPointer<ComposerEngine>` in outer capture, null-check inside `invokeMethod`.

### 2. `removeRecursively()` return ignored (`ComposerEngine.cpp:59,136`)
Silent directory deletion failure → theme install proceeds on a corrupted state.
**Fix:** Check bool return, propagate error to caller.

### 3. Hardcoded XDG paths (`ComposerEngine.cpp:132,205,223`)
`QDir::homePath() + "/.local/share/icons/"` ignores `$XDG_DATA_HOME`. Breaks non-standard installs.
**Fix:** Consistently use `QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)`.

### 4. Color parsing unchecked (`ComposerEngine.cpp:290-297`)
`toInt()` without `bool ok` check. Malformed KConfig color values silently become 0,0,0 (#000000).
**Fix:** Add `bool ok; int r = rgb[0].trimmed().toInt(&ok); if (!ok || r < 0 || r > 255) continue;`

---

## High-Priority Issues

### 5. Missing `KAboutData` (`main.cpp`)
Standard KDE requirement. Enables `--version`, `--author`, accessible About data. Currently the About dialog is hand-rolled QML with hardcoded strings.
**Fix:** Initialize `KAboutData` in `main()`, call `KAboutData::setApplicationData(about)`. Wire existing `AboutDialog.qml` to it via `Application.aboutData`.

### 6. No `KDBusService`
Multiple instances can launch silently. One user, two windows, confused state.
**Fix:** `KDBusService service(KDBusService::Unique);` in `main()`.

### 7. No `KCrash::initialize()`
Crashes produce no report, no `drkonqi` integration.
**Fix:** One line in `main()`.

### 8. Unbounded icon cache (`ComposerEngine.h:46`)
`QHash<QString,QString> m_iconCache` grows without limit. Pack-switching scenarios can cache thousands of paths.
**Fix:** Cap at ~500 entries or clear on pack change signal.

### 9. Symlink not handled in `copyDirRecursively()` (`ComposerEngine.cpp:23-50`)
Circular symlinks → stack overflow. Symlink targets duplicated instead of preserved.
**Fix:** Skip or `QFile::link()` symlinks; add depth guard parameter.

---

## Performance: What's Slow

### QML-side (biggest wins)

**P1 — `resolvePreviewPath()` called in both `visible` and `imagePath` bindings**
Every repaint calls C++ twice per FrameSvgItem. With ~7 preview elements × 2 calls = 14 C++ round-trips per frame during any animation or resize.
**Fix:** Resolve once in `Component.onCompleted` or on combo `onCurrentIndexChanged`, store in `property string`. Use the stored string in both `visible` and `imagePath`.

```qml
// Instead of:
KSvg.FrameSvgItem {
    visible: resolvePreviewPath(base, sub) !== ""
    imagePath: resolvePreviewPath(base, sub)  // called twice
}

// Do:
KSvg.FrameSvgItem {
    readonly property string _path: resolvePreviewPath(base, sub)
    visible: _path !== ""
    imagePath: _path
}
```

**P2 — `usingDefaultFallback` property (`PlasmaMixerPage.qml:299-310`)**
JS block calls C++ 4+ times, re-evaluates on any property change anywhere in scope.
**Fix:** Move evaluation to `onCurrentIndexChanged` of combo, store in plain `property bool`.

**P3 — HoverHandler `mapToItem` on every hover change in Repeaters**
6 task buttons + 3 tray icons each call `mapToItem` per hover event.
**Fix:** Precompute positions, or use simple `x/y` offset from known parent coords.

**P4 — Image loading is synchronous**
`source: "file://" + taskIconPath` blocks on file I/O.
**Fix:** Add `asynchronous: true` to all `Image` elements loading from filesystem.

### C++-side

**P5 — `findIcon()` deep scan per lookup**
`QDirIterator::Subdirectories` on an icon pack at runtime on first miss. With no pack preindexing, cold lookups on large packs (e.g. Papirus ~50k files) are slow.
**Fix:** Build a pack index on model load (`QHash<QString, QString>` name→path), invalidate when pack changes.

**P6 — `QProcess::startDetached("kbuildsycoca6")`**
Correct approach, but no feedback to user that it's running. Consider emitting a progress signal.

---

## Missing KDE Integrations

| Missing | Impact | Fix |
|---------|--------|-----|
| `KAboutData` | No `--version`, About dialog not data-driven | Add to `main()` |
| `KDBusService` | Multi-instance | 1 line in `main()` |
| `KCrash::initialize()` | No drkonqi | 1 line in `main()` |
| `ki18n_install(po)` + `po/` dir | Translations never ship | CMake + create pot template |
| AppStream screenshots | Poor store listing | Add 2-3 screenshots to XML |
| AppStream releases/urls | Incomplete metadata | Add `<releases>`, `<url>` tags |
| Window state saving | Size/position lost on restart | `KConfig` + `saveAutoSaveSettings()` or manual KConfigGroup in `onClosing` |
| Autotests | Zero coverage | `autotests/` with QtTest for ComposerEngine |

---

## Making It Lighter

1. **Remove `KF6::Svg` link from CMakeLists** — C++ never calls KSvg API directly. KSvg is used by QML engine, not the binary. Dead link dependency.

2. **Split `PlasmaMixerPage.qml` (1100+ lines)** — Extract `PreviewPanel.qml`, `ColorSection.qml`, `ComponentSelectors.qml`. QML compiler works better on smaller files; debug is faster.

3. **Replace `property var customThemeState` with typed properties** — Plain JS object disables QML optimizer. Typed `property string`/`property bool` properties compile to faster bytecode.

4. **`property var savedPresetsList: []` → C++ model** — QML var arrays bypass the QML type system. A minimal `QStringListModel` is faster and GC-friendly.

5. **Pack index at load time** — Move `findIcon()` deep scan into `IconPackListModel::populateData()`. One scan per pack instead of per-icon-lookup.

6. **Lazy-load AboutDialog / SettingsDialog** — Use `Loader { active: false }` + activate on open. Saves startup parse time for rarely-used dialogs.

---

## Architecture Verdict

The core engine design is sound. The QML state management has one structural flaw: `customThemeState` as a non-reactive JS object alongside reactive top-level properties creates two parallel systems that developers (and future contributors) will confuse. Flatten everything to typed `property` on the window object.

The preview system is clever but perf-expensive. Memoizing SVG paths per-combo-selection would cut C++ calls by ~90% without any visible change.

The app is ready for personal use and early distribution. For submission to KDE's git.kde.org it would need: KAboutData, KDBusService, po/ structure, autotests, and complete AppStream metadata.

---

## Verification

After implementing fixes:
1. `cmake .. && make -j$(nproc)` — clean build
2. Run app, open inspector (`QML_IMPORT_LOGGING=1`) — verify no repeated `resolveSvgPath` calls per frame
3. Resize window → reopen → verify size restored (KConfig test)
4. Launch second instance → verify single-instance enforcement (KDBusService test)
5. `./bin/plasma-theme-composer --version` → verify KAboutData output
