# Plan: KDE Discover Integration — Realign to "Additive Surface"

## Context

Gemini's earlier plan treated Discover as a **replacement** for our dashboard: when `useDiscover==true`, `main.cpp` would `QProcess::startDetached("plasma-discover", {"--page", "installed"})` and exit. The user clarified the intent is the opposite — **Discover should list installed AppImages IN ADDITION to our dashboard**. Clicking the app launcher entry must always open our own dashboard. The setting is purely an opt-in for AppStream XML export so Discover's "Installed" tab also sees the AppImages.

Audit ([src/main.cpp:62-64](src/main.cpp#L62-L64)) shows the only piece "missing" against Gemini's plan was the CLI redirect — but that piece **should never be added**. The redirect is contrary to user intent. Everything else in the Discover feature (XML write on install, XML remove on uninstall, retroactive sync on toggle-on, SettingsDialog checkbox) is already wired and working ([src/core/appimagemanager.cpp:77-149](src/core/appimagemanager.cpp#L77-L149), [src/core/appimagemanager.cpp:200](src/core/appimagemanager.cpp#L200), [src/core/appimagemanager.cpp:221-224](src/core/appimagemanager.cpp#L221-L224), [src/core/appimagemanager.cpp:333-363](src/core/appimagemanager.cpp#L333-L363), [src/core/appsettings.cpp:153-169](src/core/appsettings.cpp#L153-L169), [qml/SettingsDialog.qml:95-99](qml/SettingsDialog.qml#L95-L99)).

The "program not working according to plan" was a planning mismatch, not a code bug. Fix is to **realign the feature to the user's actual intent** and add the few small polish pieces the new semantics imply.

## Outcome

- Toggle reads "Also show installed AppImages in KDE Discover" — describes what it actually does.
- Clicking AppImage Manager always opens our dashboard. Discover redirect never happens.
- AppStream XML stays in sync on every dashboard open (in addition to the existing on-install / on-uninstall / on-toggle-enable points).
- Toggling on emits a `KNotification` showing how many AppImages were exposed to Discover.
- Disabling the toggle leaves existing XML in place (no purge — harmless metadata).

---

## Changes

### 1. Delete the obsolete sync call in `main.cpp`

[src/main.cpp:62-64](src/main.cpp#L62-L64) currently runs `AppImageManager::syncAppStreamMetadata()` on every dashboard launch when `useDiscover==true`. We're keeping the "sync on every dashboard open" semantic — but it belongs **inside** `DashboardWindow::open()` (or `DashboardWindow::setupAndShow()`), not in `main.cpp`. This way, `--dashboard` invocations and `KDBusService::activateRequested` re-raises both pick it up, and `main.cpp` stays focused on dispatch.

- Remove the `if (isDashboard && useDiscover()) syncAppStreamMetadata(...)` block from `main.cpp`.
- Keep the include of `core/appimagemanager.h` only if still needed; otherwise drop it.

### 2. Move per-launch sync into `DashboardWindow::setupAndShow()`

[src/gui/dashboardwindow.cpp](src/gui/dashboardwindow.cpp) — at the top of the first-show path (before or right after `m_listModel->scan()`), call:

```cpp
if (AppSettings::instance()->useDiscover())
    AppImageManager::syncAppStreamMetadata(AppSettings::instance()->applicationsPath());
```

`syncAppStreamMetadata` is already cache-aware ([src/core/appimagemanager.cpp:346-353](src/core/appimagemanager.cpp#L346-L353)) — warm-cache rows skip the cold read, so per-launch cost is a `QDir::entryInfoList` + N cache lookups + N `isDesktopLinkEnabled` checks. Acceptable.

### 3. Rename SettingsDialog toggle + tooltip

[qml/SettingsDialog.qml:95-99](qml/SettingsDialog.qml#L95-L99) — change label from "Manage installed applications via KDE Discover" to "Also show installed AppImages in KDE Discover". Add a `ToolTip` clarifying: "Exports AppStream metadata so Discover's Installed tab lists your AppImages. The AppImage Manager dashboard remains available."

i18n: change the existing `i18n("...")` string. The translator-comment line above it (`i18nc(...)` if present) needs the same update. Treat the rename as a string change — the existing `.po/.pot` workflow handles it.

### 4. Notify on toggle-enable

[src/core/appsettings.cpp:153-169](src/core/appsettings.cpp#L153-L169) — `setUseDiscover(true)` already calls `syncAppStreamMetadata`. Wrap that call to count written files and fire a `KNotification`.

Option A (minimal change): add an overload `int syncAppStreamMetadata(const QString &appsDir)` that returns the count of XML files written (incremented inside the `createAppStreamXml(info)` branch at [src/core/appimagemanager.cpp:358-360](src/core/appimagemanager.cpp#L358-L360)). Old void caller paths can ignore the return.

Option B (cleaner, recommended): change the existing signature to return `int`. Only two call sites (`appsettings.cpp:setUseDiscover`, and the new one inside `DashboardWindow`). Use `[[maybe_unused]]` or `(void)` where the count isn't needed.

In `setUseDiscover(true)` after sync:

```cpp
KNotification *n = new KNotification(QStringLiteral("discoverSyncDone"));
n->setComponentName(QStringLiteral("appimagemanager"));
n->setTitle(i18n("AppImage Manager"));
n->setText(i18np("%1 AppImage is now visible in KDE Discover.",
                 "%1 AppImages are now visible in KDE Discover.",
                 count));
n->setIconName(QStringLiteral("appimagemanager"));
n->sendEvent();
```

`KNotification` is already linked via `KF6::Notifications` ([src/CMakeLists.txt:128](src/CMakeLists.txt#L128)) and used in `AppImageListModel::sendError` / `UpdateDaemon`. No new dependency.

### 5. No purge on disable

Confirmed by user: leave any previously written `~/.local/share/metainfo/appimage_*.appdata.xml` alone when `useDiscover` flips off. They will continue to be removed on AppImage uninstall via the existing `removeDesktopLink()` path ([src/core/appimagemanager.cpp:221-224](src/core/appimagemanager.cpp#L221-L224)). Document this in a brief comment on the setter. **No code change** beyond the comment.

### 6. Documentation update

[CLAUDE.md](CLAUDE.md) and [GEMINI.md](GEMINI.md) — add a short subsection under "Key Conventions" titled "Discover integration" describing:
- The setting is additive, not exclusive.
- XML written on install, removed on uninstall, plus a per-dashboard-open re-sync when the setting is on.
- `AppImageManager::syncAppStreamMetadata` and `appDataFilePath` are the entry points.

---

## Files to modify

| File | Change |
|------|--------|
| [src/main.cpp](src/main.cpp) | Remove lines 62-64 (and now-unused `#include "core/appimagemanager.h"` if applicable) |
| [src/gui/dashboardwindow.cpp](src/gui/dashboardwindow.cpp) | Call `syncAppStreamMetadata` at top of `setupAndShow` when toggle is on |
| [src/core/appimagemanager.h](src/core/appimagemanager.h) | Change `syncAppStreamMetadata` return type `void` → `int` |
| [src/core/appimagemanager.cpp](src/core/appimagemanager.cpp) | Return written-count from `syncAppStreamMetadata`; same in `createDesktopLink` it stays `void` |
| [src/core/appsettings.cpp](src/core/appsettings.cpp) | `setUseDiscover(true)` path: capture count, fire `KNotification` with `i18np` plural; brief comment about no-purge |
| [qml/SettingsDialog.qml](qml/SettingsDialog.qml) | Rename label, add tooltip |
| [CLAUDE.md](CLAUDE.md) / [GEMINI.md](GEMINI.md) | Add "Discover integration" subsection |

No CMake changes. No new files. No new dependencies.

---

## Verification

1. Build: `cmake --build --preset dev` → zero errors. `ctest --test-dir build/dev --output-on-failure` → all pass (existing `tst_appimagereader.cpp:64-70` already covers `appDataFilePath`).
2. Install: `sudo cmake --install build/dev`.
3. Launch `appimagemanager` (no args) with `useDiscover=false` → dashboard opens, **no** `metainfo/appimage_*.appdata.xml` writes (check `ls ~/.local/share/metainfo/`).
4. Open Settings → flip "Also show installed AppImages in KDE Discover" on → KNotification fires with count → check `ls ~/.local/share/metainfo/appimage_*.appdata.xml` shows one file per installed AppImage.
5. Open `plasma-discover --page installed` → installed AppImages appear with icon, version, description.
6. From within Discover, click "Remove" on an AppImage → AppImage moves to trash, desktop file gone, XML gone, dashboard reflects removal via KDirWatch.
7. Install a new AppImage from Dolphin → ManageWindow → drag-to-install → check XML appears under `metainfo/` immediately (covered by `createDesktopLink`).
8. Close dashboard, manually drop an AppImage into `~/Applications`, re-open dashboard → KDirWatch + the new `setupAndShow` sync write the XML → Discover refresh shows it.
9. Toggle Discover setting OFF → existing XML files **remain** (verify with `ls`). Uninstall one of those AppImages → its XML disappears.
10. Run `appimagemanager --dashboard` while a dashboard instance is open → existing window raises (`KDBusService::Unique`); the per-launch sync inside `setupAndShow` does not re-fire (because we open only once); acceptable since the install/uninstall and toggle-enable paths cover correctness.
