# CLAUDE.md

This file is the authoritative reference for working on AppImage Manager.

## Project Overview

> **Disclaimer:** This project and its author are in no way affiliated with KDE or the KDE e.V. organization. While the project's goal is to provide a native-like experience on the Plasma desktop, it is an independent, community-driven project authored by the user. It should not pretend to be an official KDE product (e.g., `org.kde.*`).

**AppImage Manager** is a lightweight KDE Plasma 6 utility for installing, managing, updating, and removing AppImage files. It integrates with Dolphin through Open With (the `.desktop` file's `MimeType=`) and provides a standalone dashboard for browsing, batch-managing, and updating all installed AppImages.

**Distribution target:** KDE Store and AUR.  
**Philosophy:** Simplicity, efficiency, and complete source agnosticism. No unnecessary abstractions. Every feature must justify its existence. The utility strives to remain entirely agnostic to where AppImages are sourced from, supporting custom repository configurations natively.  
**Stack:** C++20, Qt 6.9, KDE Frameworks 6.7, KirigamiAddons, Kirigami (Plasma 6 era), QML. Project version `2.3.0`.  
**License:** GPL-2.0-or-later.

### AI Assistant Guidelines

- **Plan Backups:** Always "backup" every big change or implementation plan as a `.md` file inside the `markdown/plans/` directory of the project.
- **Ask, don't guess.** The user prefers too many questions over too few. When a choice is the user's (UX, scope, what to remove), ask before acting.
- **Verify APIs, don't guess them.** Check Qt/KF6/Kirigami names against the installed headers or QML modules (`/usr/include/qt6`, `/usr/include/KF6`, `/usr/lib/qt6/qml`) before using them. Several pitfalls below came from plausible-looking APIs that do not exist.
- **Writing style:** in `README.md`, do not use em dashes (—). Use regular dashes (-) instead.
- **Todo.md maintenance:** open tasks in [Todo.md](Todo.md) are grouped by priority (High / Medium / Low), not feature area. When adding a task, ask which priority it belongs in rather than guessing. When a task is done, move it to the **Completed** section (grouped by feature area there) instead of marking it done in place.
- **CHANGELOG.md:** add a line under `## Unreleased` for every user-visible change.

#### Behavioral principles (derived from Karpathy's LLM coding observations)

**1. Think before coding.** State assumptions explicitly before implementing. If multiple interpretations exist, surface them — don't pick silently. If something is unclear, stop and ask. Push back when a simpler approach exists.

**2. Simplicity first.** Minimum code that solves the problem. No features beyond what was asked. No abstractions for single-use code. No speculative flexibility or configurability. If it could be 50 lines, don't write 200.

**3. Surgical changes.** Touch only what the request requires. Don't improve adjacent code, comments, or formatting. Match existing style even if you'd do it differently. Mention unrelated dead code — don't delete it. Remove only imports/variables/functions that *your* changes made unused.

**4. Goal-driven execution.** Transform tasks into verifiable goals before starting. For multi-step tasks, state a brief plan with per-step success criteria. Loop until verified — don't hand back unverified work.

### Roadmap

Planned work lives in [Todo.md](Todo.md).

---

## Build Commands

```bash
# Configure + build (dev — Debug, installs to /usr, exports compile_commands.json)
cmake --preset dev
cmake --build --preset dev

# Install to /usr
sudo cmake --install build/dev


# Install as a pacman package from this checkout (replaces the old install.sh)
cd packaging && makepkg -si

# Release build
cmake --preset release
cmake --build --preset release
sudo cmake --install build/release

# Clean rebuild
rm -rf build/ && cmake --preset dev && cmake --build --preset dev

# Run unit tests
ctest --test-dir build/dev --output-on-failure

# Syntax-check a single QML file headlessly (124 = ran fine until timeout, 2 = real error).
# Confirms syntax/imports only, not runtime behaviour — still test live.
QT_QPA_PLATFORM=offscreen QML_IMPORT_PATH=build/dev/src timeout 5 qml6 qml/<File>.qml; echo $?
```

**When releasing:** bump `VERSION` in the top-level `CMakeLists.txt` (`project(AppImageManager VERSION X.Y.Z ...)`). This is the single source of truth — `version.h` is generated from it and `AppSettings::appVersion()` reads it at runtime. Also update the version string in this file, move `CHANGELOG.md`'s `## Unreleased` entries under the new version, and add a `<release>` entry to `data/appimagemanager.metainfo.xml`. For the AUR, bump `pkgver` in `packaging/appimage-manager-plasma/PKGBUILD`, update `sha256sums` once the tag is on GitHub (`updpkgsums`), and regenerate `.SRCINFO` in both package folders (`makepkg --printsrcinfo > .SRCINFO`). Forgetting any of these causes the About dialog to display the wrong version or the KDE Store validator to reject the submission.

Build output: `build/<presetName>/`. Unit tests in [tests/](tests/) cover `appimageinfo` helpers, `AppImageReader` cleanName, `AppImageCache` round-trip, `AppImageListModel` watcher behavior, the daemon's schedule and tray model (`tst_updatedaemon`), built-in zsync (`tst_zsync`), app menu entries (`tst_desktoplink`), and update sources and launch environment (`tst_updatesource`: input auto-detection, effective update info, config round-trip incl. the legacy `Repo` key, release asset pick, environment parsing, Content-Type guard). The manual checklist below still applies for UI/integration paths.

---

## Repository Layout

| Path | Contents |
|------|----------|
| [src/core/](src/core/) | Pure C++ — reader, cache, manager, settings, GitHub checker, daemon |
| [src/gui/](src/gui/) | Qt Quick backend classes (windows, models, providers, update manager, download watcher) |
| [src/dbus/](src/dbus/) | D-Bus adaptor exposing read-only model state |
| [qml/](qml/) | All QML files (top-level, NOT under `src/`) |
| [tests/](tests/) | Unit tests (`tst_*.cpp`) |
| [data/](data/) | Desktop files, icons, autostart, .pot |
| [packaging/](packaging/) | `PKGBUILD` (this checkout, `makepkg -sif`), `appimage-manager-plasma/` (latest release, published to the AUR), `appimage-manager-plasma-git/` (latest commit, published to the AUR) |

---

## Architecture — Build Targets

| Target | Type | Description |
|--------|------|-------------|
| `appimagemanager_core` | Object library | Pure C++, zero Qt Quick dependency. Reader, cache, manager |
| `appimagemanager_qml` | Shared lib + QML module `appimagemanager` | All GUI backend classes + the D-Bus adaptor + the QML files |
| `appimagemanager_bin` → `appimagemanager` | Executable | No args → dashboard. With file arg (Open With) → manage window. `--daemon` → background update + download watcher. `--run <file> [args]` → start an AppImage with its settings (sandbox), no GUI |

The binary links `appimagemanager_qml` and has three entry points: manage window (a file argument, from Dolphin's Open With), dashboard, daemon. There is no Dolphin plugin or service menu (removed after 2.1.0: Open With is the one way in).

---

## Class Map

### Core (`src/core/`) — no Qt Quick

| Class | Files | Role |
|-------|-------|------|
| `AppImageInfo` | `appimageinfo.h` | Value struct: `originalName`, `cleanName`, `appId`, `appName`, `version`, `categories`, `comment`, `description`, `developerName`, `homepage`, `execArgs`, `fileSize`, `iconData`, `iconExt`, `updateInfo`, `desktopEntry` (the app's own embedded `.desktop` text), `isValid`. Also defines the `SignatureState` enum (`Unchecked=0`, `Valid=1`, `Invalid=2`, `Unsigned=3`, `GpgUnavailable=4`) used by `AppImageReader::verifySignature()`. `description` = AppStream XML content; falls back to `comment` (`.desktop Comment=`) if XML absent |
| `AppImageReader` | `appimagereader.h/.cpp` | **BLOCKING** extractor. Requires `libappimage` (in-process SquashFS, no FUSE/subprocess). Reads `.desktop` via `KDesktopFile`, then AppStream XML from `usr/share/metainfo/*.appdata.xml` or `usr/share/appdata/*.metainfo.xml` via `QXmlStreamReader`. **Always call via `QtConcurrent::run`**. Also provides `static SignatureState verifySignature(path, QString *fingerprint)` — the appimagetool `--sign` format: armored `.sha256_sig` over the hex SHA-256 of the file with `.sha256_sig`/`.sig_key` (and, tried both ways, `.digest_md5`) zeroed, with and without a trailing newline; `.sig_key` imported into a temporary keyring. The runtime reserves these sections zero-filled, so all-zero = `Unsigned` (before 2.1.1 that read as `Invalid`). `GpgUnavailable` without gpg, `Unchecked` when signed with a key we don't have. Tested with an objcopy-made, gpg-signed ELF (`tst_appimagereader`). Safe to call from worker threads |
| `AppImageCache` | `appimagecache.h/.cpp` | Thread-safe on-disk cache (SQLite via `Qt6::Sql`, WAL mode, per-thread connection; DB path `$XDG_DATA_HOME/AppImageManager/cache.db`; one row per AppImage keyed by `MD5(path)`). **`kCacheVersion = 6`** — schema migrated from QSettings INI in v4 (adds `developer_name` / `homepage`); v5 forces a re-read after the appId fix; v6 adds `desktop_entry` (an `ALTER TABLE` adds the column to older databases). Rows with `cache_version != 6` or stale `mtime` are returned as invalid (forces a cold re-read) |
| `AppImageManager` | `appimagemanager.h/.cpp` | File operations: `installAppImage()` (KIO::move when source is under `~/Downloads`, KIO::copy otherwise — preserves originals on USB sticks / project dirs; chmod +x on result), `createDesktopLink()` (reuses an entry the app wrote, keeping its other keys; our own entry copies the app's `MimeType`, `Keywords`, `StartupWMClass`, `StartupNotify`, `GenericName`, `Name`/`Comment` with every translation, and its `Actions` (each action's `Exec` = our launch command + the action's arguments), plus our action `appimagemanager-manage` → `appimagemanager --select <file>`, and marks it `X-AppImageManager-Entry=2`; `desktopLinkOutdated()` finds our pre-2.3 entries, which `AppImageListModel::applyMetadata` rewrites once), `removeDesktopLink()` (every entry that starts the AppImage; ours deleted, others trashed), `isDesktopLinkEnabled()` (any visible entry whose `Exec` starts the AppImage, via a directory-mtime-cached index of `~/.local/share/applications`), `removeDuplicateDesktopLinks()` (drops our `appimage_*` entries when another entry starts the same AppImage; run on every metadata load), `findCorpses()` (blocking), `removeItems()` (KIO::trash). `rebuildSycoca()` + `update-desktop-database` + `update-icon-caches` called from `createDesktopLink`/`removeDesktopLink` — covers both Plasma's service catalog and XDG-spec caches for non-Plasma launchers. `fileKey(info)` derives icon and fallback desktop filenames: the cleanName slug plus a 6-char `qHash(originalName)` suffix (prevents collisions between two AppImages with the same display name) |
| `AppSettings` | `appsettings.h/.cpp` | QML singleton (`AppSettings`). Delegates all reads/writes to the KConfig XT-generated `AppImageManagerSettings::self()` (schema: `appimagemanagersettings.kcfg`). Properties: `applicationsPath`, `showDisclaimer`, `firstRun`, `showNotifications`, `updateFrequency`, `customUpdateDays`, `watchDownloads`, `showInstallBox`, `showOverview`, `configInApplications` (new installs keep settings in Applications), `verifySignatures`, `sandboxAll` / `sandboxFolders` (Settings → Security; `sandboxAvailable` CONSTANT), `libraryListWidth` (0 = default), `discoverEnabled`. Each setter calls `AppImageManagerSettings::self()->save()`. Setter validates path via `QDir::mkpath()`, emits `applicationsPathError(msg)` on failure |
| `ReleaseChecker` | `releasechecker.h/.cpp` | `check(filePath, embeddedUpdateInfo, currentVersion)`: applies the user's update source (`updatesource.h`) and handles `gh-releases-zsync\|owner\|repo\|tag\|pattern` (GitHub: the public `releases.atom` feed first (`feedNewestTag()`, the tag from the entry link), the API (`/releases/latest`, or `/releases` with pre-releases) only when the feed shows a newer release or fails; Atom feed fallback when the API fails), `forgejo-releases\|host\|owner\|repo` (Codeberg/Forgejo/Gitea `/api/v1/repos/o/r/releases[/latest]`, same JSON) and `direct\|url` (HEAD request; Content-Type must not be a web page; Last-Modified, else ETag, is the build marker shown as the version and stored as `installedTag`; without a stored marker the local file's mtime is compared). Emits one `checkFinished(Result, newVersion, url)`; on failure `errorText()` says why in words (repository not found, request limit, no connection, no releases, no AppImage for this computer, web page, …). `url` is a `.zsync` or `.AppImage` asset picked by `pickAsset()` (ported from Shelly: `.zsync` first, glob pattern, CPU architecture aliases matched as whole tokens (`Swarm` is not `arm`, `x86_64` is not `x86`), version-stripped name match against the local file; ambiguous = `Failed`). Version rule: remote tag == stored `installedTag` → up to date; unknown local version → update available; else `isNewerVersion`. `isNonBinaryContentType()` is shared with the download path. Used by both `UpdateDaemon` and `AppImageUpdateManager` |
| `UpdateSource` | `updatesource.h/.cpp` | Per-AppImage update source chosen by the user: `type` (`GitHub` "owner/repo", `Forgejo` "host/owner/repo", `Zsync` URL, `Url` direct download), `value`, `prerelease`, plus `installedTag` written after a successful update. `isPinned()`/`setPinned()`: "Don't Update" in `[Pinned][<file name>]` (separate from the source, carried over by Replace); pinned items are skipped by `AppImageUpdateManager::checkForUpdates` and get state `Pinned`. Stored in `appimagemanagerrc` `[UpdateSources][<file name>]` as `Type`/`Value` (a legacy `Repo` key reads as GitHub), re-read on every load (dashboard writes, daemon reads). `parseUpdateSourceInput(input, forced)` auto-detects the kind; `effectiveUpdateInfo()` maps an override to `gh-releases-zsync\|`, `forgejo-releases\|`, `zsync\|` or `direct\|`; `isSupportedUpdateInfo()`, `isReleaseUpdateInfo()` (everything ReleaseChecker handles), `describeUpdateInfo()` ("Codeberg · o/r"), `updateSourceWebUrl()` (repository page for GitHub/Forgejo sources; itemData `updateSourceUrl`, a `Kirigami.UrlButton` in Details) |
| `checkresults` | `checkresults.h/.cpp` | Update check results shared by the dashboard and the daemon (separate processes) through the state config `appimagemanagerstaterc`: `[Tray] LastCheck`, `[TrayResults] <file name>` = state, new version, url, version at check, reason (2.1.0's four-field entries still load). `UpdateState` enum (NotChecked, UpToDate, UpdateAvailable, NoSource, Failed, Updating, Pinned; same values as `TrayModel::State` and the QML constants; Pinned is never stored, it follows `isPinned()`). `loadCheckResults()` / `saveCheckResults()` / `saveCheckResult()` (only UpToDate/UpdateAvailable/Failed are stored), `loadLastCheck()` / `saveLastCheck()`, `formatCheckTime()` ("14:02", "yesterday 14:02"). Writers tell the other process: dashboard → daemon `ReloadState()`, daemon → dashboard `UpdatesChanged` |
| `sandbox` | `sandbox.h/.cpp` | Per-AppImage bubblewrap sandbox in `[Sandbox][<file name>]` Enabled/Folders/Network (Network absent = allowed; off adds `--unshare-net`, tested by reading the sandbox's `/proc/net/dev`); no `Enabled` key = follow `sandboxAllEnabled()` (Settings → Security, only with bwrap; `saveSandbox()` drops a choice equal to it), `sharedSandboxFolders()` are added to every sandboxed app's folders. `sandboxAvailable()` is looked up once per process. The list model rewrites all menu entries when `sandboxAll` changes. `sandboxArguments()` (pure, tested with a real bwrap run): `/usr`, `/etc`, `/opt`, `/sys` read-only, `/bin` etc. as links, `/dev`, new `/proc`, tmpfs `/tmp` (+ X11 socket), the runtime dir (Wayland, sound, session bus), tmpfs home with the desktop look read-only (kdeglobals, gtk, fontconfig, fonts, icons), the settings folder and the given folders read-write, the AppImage and its mount read-only, then `<mount>/AppRun`. `runAppImage()` = `appimagemanager --run <file> [args]` (handled in `main.cpp` before `QApplication`): environment, then for sandboxed apps `--appimage-mount` outside (FUSE doesn't work inside), bwrap with `APPIMAGE`/`APPDIR`/`ARGV0`/`OWD`, `XDG_DATA_HOME`/`XDG_CACHE_HOME` inside the settings folder; waits, then unmounts. `launchCommand()` is `--run` for sandboxed apps and is used by `launchWithEnvironment()` and `createDesktopLink()`; `execTarget()` sees through `appimagemanager --run <file>`. Turning the sandbox on gives the app a settings folder |
| `releasenotes` | `releasenotes.h/.cpp` | `releaseListUrl(updateInfo)` (GitHub `/releases?per_page=20`, Forgejo `/api/v1/…/releases?limit=20`), `pickReleaseNotes(json, installedVersion, installedTag, prereleases)` (drafts out, pre-releases unless allowed, every release newer than the installed one, stopping at `installedTag`; none newer → the newest), `cleanReleaseBody()` (images and HTML comments removed: remote images would load). Fetched on demand by `AppImageListModel::fetchReleaseNotes(row)` → `releaseNotesReady(path, notes, pageUrl, error)`; errors via `ReleaseChecker::describeReplyError()` |
| `previousversion` | `previousversion.h/.cpp` | Undo for updates. `AppImageUpdateManager`'s swap moves the replaced file (the `.bak`) to `<folder>/.previous/<file name>` via `keepPrevious()` (one per AppImage; version passed to `downloadUpdate(…, currentVersion)`, the pre-update `installedTag` saved) in `[Previous][<file name>]` Version/Tag. `restorePrevious(path, currentVersion)` swaps file, version and tag both ways. The kept file is a leftover on Remove and is trashed by Replace |
| `launchenvironment` | `launchenvironment.h/.cpp` | Settings folder: `[ConfigFolder][<file name>] = <name>` → `<folder of the AppImage>/.config/<name>/` (`configFolderName/Path`, `defaultConfigFolderName()` = file name without OS/CPU words and version); `launchVariables()` = saved environment + `XDG_CONFIG_HOME` (the folder) + `XDG_CONFIG_DIRS` (`~/.config` first, for theme/fonts) unless the user set them, used by launches and the `.desktop` `Exec=`. New installs get a folder when `AppSettings::configInApplications` is on (backend + dashboard install); Replace carries the entry over; the folder is a leftover on Remove. Per-AppImage launch environment (`KEY=value` list) in `[Environment][<file name>]`. `parseEnvironment()` (one per line, `#` comments, invalid/duplicate lines reported), `launchWithEnvironment()` (used by the dashboard and manage window), `environmentExecPrefix()` (`env 'K=v' …` prepended to the `.desktop` `Exec=` by `createDesktopLink`) |
| `UpdateDaemon` | `updatedaemon.h/.cpp` | Background part, `--daemon` (autostart desktop file in `$KDE_INSTALL_AUTOSTARTDIR`). **Settings** are re-read every tick (`reloadSettings()`: the main window writes them in its own process; a new Applications folder is re-watched and re-scanned). **Schedule:** a 1-minute tick compares now with `LastCheck` from the state config (`KSharedConfig::openStateConfig()`, `~/.local/state/appimagemanagerstaterc`, group `[Tray]`) via the pure, tested `intervalDays()` / `nextCheck()` / `isDue()` (no long `QTimer`: it overflows past 24.8 days). Skips while `QNetworkInformation` says offline; a failed scheduled check retries after 30 min. **Checks and downloads** go through its own `AppImageUpdateManager` (all source types) with per-item `itemChecked`; results persisted through `checkresults` (shared with the dashboard) so the tray is right after login; `ReloadState()` takes over results the dashboard wrote (and marks its updates as announced). **Notifications:** one summary for versions not yet in `[TrayAnnounced]`, actions Update All / Open; Update All ends with one "Updated N" summary (`setCompletionNotifications(false)`). **Tray** (`KStatusNotifierItem`): our icon, count badge drawn with `setIconByPixmap` when updates wait, Passive when idle; tooltip with status + schedule; left click toggles the tray window (`TrayWindow.qml`, own engine; on Wayland a LayerShellQt surface anchored to the screen corner nearest the click, else a frameless window at the click), middle click checks, right-click menu Open / Check for Updates / Settings… (+ KSNI's Quit). Opening the main window runs our own binary with `--select <path>` / `--settings`. Watches `applicationsPath` with `KDirWatch` (installs, removals, updates done elsewhere; a changed file with a pending update is re-checked). Owns a `DownloadWatcher` for `~/Downloads`. D-Bus: service `io.github.appimagemanager.Daemon` (also tells the dashboard to skip duplicate download notifications; D-Bus-activatable via `io.github.appimagemanager.Daemon.service`), object `/Daemon`, interface `io.github.appimagemanager.Daemon1`: `State()` (JSON for the tray widget; app icons as files in `~/.cache/appimagemanager/tray-icons/`), `CheckNow()`, `UpdateAll()`, `OpenManager(path)`, `OpenSettings()`, `ReloadState()`, `RegisterView()`/`UnregisterView()` (tray widget present: no KStatusNotifierItem; the caller's bus name is watched so the icon returns if plasmashell leaves), signals `UpdatesChanged`, `StateChanged` (coalesced 250 ms). The KStatusNotifierItem appears 8 s after start only if no widget registered |

### GUI (`src/gui/`) — Qt Quick dependency

| Class | Files | Role |
|-------|-------|------|
| `AppImageBackend` | `appimagebackend.h/.cpp` | QML context property `backend`. Owns `AppImageInfo` + all mutable install state. Exposes metadata + ops as Q_PROPERTYs and slots. Holds `CorpseModel`. Emits `uninstallFinished`. Install flow is split: `beginInstall()` runs `AppImageReader::verifySignature()` off-thread (if `AppSettings::verifySignatures` is on) and emits `signatureCheckReady(int state)` — QML inspects the `SigState` Q_ENUM value and either calls `doInstall()` directly or shows a warning dialog first. `SigState` enum (Unchecked=0, Valid=1, Invalid=2, Unsigned=3, GpgUnavailable=4) mirrors `SignatureState` and is registered as Q_ENUM for QML access. After metadata loads (not installed) a worker finds the copy already in Applications (same file name first, else the newest copy with the same `appId`, or the same `appName` when either lacks an id): `existingState` (`ExistingState`: NotInstalled, ThisNewer, SameVersion, ThisOlder, VersionUnknown), `existingVersion`, `existingSameFile`. `beginInstall(replace)`: replace with the same file name trashes the old copy first; otherwise it installs, moves update source + environment to the new file name (not `InstalledTag`), removes the old menu entry before writing the new one (both may be the same app-id file), trashes the old copy, and keeps the app menu entry only if the old one had it |
| `AppImageIconStore` | `appimageiconstore.h/.cpp` | Process-wide singleton holding raw icon bytes extracted from AppImages. Shared across all QML engine instances (dashboard + manage windows) so multiple windows don't re-extract the same bytes. Thread-safe write (`setIconData`) via internal lock |
| `AppImageIconProvider` | `appimageiconprovider.h/.cpp` | `QQuickImageProvider` for `image://appimage/<id>`. Reads raw bytes from `AppImageIconStore`; each provider instance owns its own per-engine `QPixmap` render cache (`m_cacheMutex` guards the `QCache` — `QCache::object` mutates LRU state even on read). SVG/PNG rendering happens between the store read and cache write. Caches rendered pixmaps keyed by `(id, size)`. Key `"icon"` = manage window single icon; key = `qHash(path)` decimal string = dashboard per-file icon |
| `CorpseModel` | `corpsemodel.h/.cpp` | `QAbstractListModel` for leftover config/cache dirs. Roles: `filePath`, `fileSize`, `isChecked`. **No `QML_ELEMENT`** — Qt 6.11 constexpr metaobject bug. Exposed only via `AppImageBackend::corpseModel` Q_PROPERTY. `checkedRole` Q_PROPERTY replaces enum access from QML |
| `AppImageWindow` | `appimagewindow.h/.cpp` | One `QQuickWindow` per AppImage path. Deduplicates via `static QHash<QString, AppImageWindow*> s_instances`. Re-opening same path raises existing window |
| `DownloadWatcher` | `downloadwatcher.h/.cpp` | Shared `~/Downloads` watcher. `KDirWatch` in `WatchFiles` mode + settle pass (poll `size`/`mtime` until stable) so we never notify mid-download. Emits `appImageFound(path, displayName)` once per file. Used by both `UpdateDaemon` and `AppImageListModel`; the dashboard suppresses its own listener while the daemon owns the well-known D-Bus name. `isSandboxed()` returns `true` when `isRunningInSandbox()` detected at construction time (env vars `FLATPAK_ID` / `SNAP`); `setEnabled(true)` is a no-op in that case — KDirWatch cannot watch paths outside the sandbox |
| `ZsyncDownloader` | `zsyncdownloader.h/.cpp` | Built-in zsync (see the pitfall). `parseControl`, `matchBlocks`, `missingRanges` are static and tested; `start()` fetches the control file, matches off-thread while copying found blocks into `<file>.new`, fetches the rest, checks SHA-1, `finished(ok)`; `targetUrl()`/`sha1()` for the fallback |
| `AppImageUpdateManager` | `appimageupdate.h/.cpp` | Batched update checker + downloader. `checkForUpdates(items)` — replaces each item's update info with `effectiveUpdateInfo()` and drops unsupported ones; release-type items go through `ReleaseChecker`, plain `zsync\|` items fetch the `.zsync` header and compare `SHA-1` (calculated off-thread via `QtConcurrent`); emits `updateFound(path, version, url)`. Up to 6 items are checked in parallel (`kMaxParallelChecks`), with a 30 s watchdog that restarts whenever one finishes (`checkFinished(-1, 0)` when none finished for 30 s); `m_batch` is bumped per batch and on time-out so late replies are dropped. `downloadUpdate(path, url, ...)` — a `.zsync` URL goes through the built-in `ZsyncDownloader` (on failure: the whole file from the control file's `URL:`, SHA-1 checked); any other URL is a direct `.AppImage` HTTP download (`downloadBinary`), aborted when the first response chunk has a web-page Content-Type. Both paths use an atomic-ish swap: `old → old.bak`, then `old.new → old`, then the `.bak` is kept as the previous version (`keepPrevious()`, removed only if that fails). If either rename fails, restores `.bak` to original and emits failure. A leftover `.bak` after a crash = mid-swap interruption, recoverable manually. Signals: `checkingChanged`, `checkProgress(done, total)` (finished items / items with a source), `checkFinished(found, failures)`, `updateFound`, `itemChecked(path, CheckResult, reason)` (once per checked item), `downloadProgress`, `downloadFinished`. `setCompletionNotifications(false)` turns off the per-app "Update Completed" notification (the daemon sends a summary) |
| `AppImageListModel` | `appimagelistmodel.h/.cpp` | Dashboard list model. Roles: `filePath`, `displayName`, `cleanName`, `appName`, `version`, `iconSource`, `hasDesktopLink`, `metadataLoaded`, `appSize`, `formattedSize`, `addedDate`, `categories`, `comment`, `description`, `developerName`, `homepage`, `updateAvailable`, `updateVersion`, `isUpdating`, `updateProgress`, `sectionCategory` / `categoryLabel` (readable freedesktop main category, "Other"), `sectionAdded` (Today / This Week / This Month / Earlier), `updateState` (UpdateState), `updateReason`, `olderCopyOf` (newest copy's name when several AppImages share an appId), `listSection` (header for `sectionMode`: 0 none, 1 category, 2 date, 3 status; rows with an update always under "Updates available", the rest under "Installed" in mode 0). `itemData()` also carries `environment` and `updateSourceLabel`. State comes from `checkresults` (matched by file name and version) or the dashboard's own check (`itemChecked`); after a check or update the results are written back and the daemon is told (`ReloadState`); `UpdatesChanged` from the daemon re-reads them. Properties: `scanning`, `checkingUpdates`, `checkDone` / `checkTotal` (update check progress), `upToDateCount` / `failedCount` / `notCheckedCount` / `noSourceCount` / `totalSizeText` / `lastCheckTime` (the overview), `sectionMode`, `updateCount` (int — rows with an update available; drives the "Installed (N)" tab and Update All), `downloadWatcherSandboxed` (bool `CONSTANT` — true when `FLATPAK_ID`/`SNAP` env present). Invokables: `scan`, `refresh`, `toggleDesktopLink`, `requestRemoveAt`, `requestLaunch`, `checkForUpdates`, `downloadUpdate`, `itemData(row)` (bulk role snapshot for QML), `updateSource(row)` / `setUpdateSource(row, input, type, prerelease)` / `describeUpdateSourceInput(input, type)` (per-AppImage update source; saving resets the row's update state, the user re-runs Check for Updates). `environment(row)` / `setEnvironment(row, text)` (launch environment; rewrites the app menu entry). `checkSignature(row)` (background, `m_readerPool`, once per file version; itemData `signatureState`/`signatureKey`; LibraryPage calls it on selection). `sandbox(row)` / `setSandbox(row, enabled, folders)` (rewrites the app menu entry). `restorePrevious(row)` (swap with the kept previous version, row back to NotChecked). `configFolder(row)` / `setConfigFolder(row, enable, moveDirs)` (settings folder; moves the chosen folders with `KIO::move`, never over an existing name). On a successful release-type update it stores the installed tag via `saveInstalledTag()`. Watches `applicationsPath` via `KDirWatch` (`WatchFiles`) and routes `created/deleted/dirty` straight to incremental inserts/removes — no debounce, no full re-scan. Reader workers dispatch through `m_readerPool` (bounded to `qBound(1, QThread::idealThreadCount() / 2, 4)`) to prevent thread storms on large libraries. Holds an `AppImageUpdateManager` and a `DownloadWatcher`. `m_generation` invalidates stale futures on `refresh()`; `m_pendingLoads` (in-flight metadata futures) drives the `scanning` flag. `m_pathIndex` (`QHash<QString, int>`) keeps row index by path for O(1) `findRowByPath` |
| `AppImageSortFilterModel` | `appimagesortfiltermodel.h/.cpp` | Proxy over `AppImageListModel`. Sort: `SortByName=0`, `SortBySize=1`, `SortByCategory=5`, `SortByDate=9`, `SortByStatus=10` (`statusRank`: updates, failed, up to date, not checked, no source); rows with an update come first under every sort. Filter: case-insensitive text match on `cleanName` + `appName`, and `statusFilter` (an UpdateState, -1 = all; set from the overview). Qt's proxy re-sorts and re-filters on any role change, so state changes move rows live. `lessThan` reads typed values via `AppImageListModel::sortKey()` to skip QVariant role dispatch. Uses Qt 6.9 `beginFilterChange/endFilterChange` where applicable |
| Tray widget | `data/plasmoid/` | Plasma system tray widget (lists updates first, folds apps without an update source into "Not tracked (n)", shows the failure reason) `io.github.strandzen.appimagemanager.tray` (`X-Plasma-NotificationArea`, enabled by default), pure QML, installed to `share/plasma/plasmoids/`. Only a view: reads the daemon's `State()` JSON over D-Bus (`org.kde.plasma.workspace.dbus`: `SessionBus.asyncCall`, `SignalWatcher` → `dbusStateChanged()`, poll every 2 s while open / 60 s otherwise) and calls `CheckNow`/`UpdateAll`/`OpenManager(path)`/`OpenSettings`. Calls `RegisterView` on load (the daemon then drops its own KStatusNotifierItem) and `UnregisterView` on destruction. Strings via `i18nd("appimagemanager", …)` from our catalog. Passive status when nothing waits |
| `TrayModel` | `traymodel.h/.cpp` | Plain `QObject` holding the tray's list (path, name, icon (theme name or the cached icon file), version, state (`TrayModel::State`, same values as `UpdateState`), new version, url, reason, progress), sorted updates first and apps without a source last. Summary properties for QML (`trayModel`): `updateCount`, `updatingCount`, `checking`, `scheduleText` (last + next check, tooltip), `lastCheckText` (popup header); signals `changed` (the list) and `summaryChanged`. The list reaches both tray views as `UpdateDaemon::State()` JSON. Filled by `UpdateDaemon` |
| `DashboardWindow` | `dashboardwindow.h/.cpp` | Singleton dashboard host. `open(page, selectPath)` (from `--settings` / `--welcome` / `--select`, also forwarded to a running instance through `KDBusService::activateRequested`) calls the QML `openRequest()`. Creates its own `QQmlApplicationEngine`. Holds `m_listModel`, `m_proxyModel`, `m_uninstallBackend`, `m_storeModel`. `createBackend(path, withCorpses)` — uninstall backend passes `true` (triggers corpse scan after metadata load). Exposes `amStoreModel` as context property |
| `AMStoreModel` | `amstoremodel.h/.cpp` | Browse tab store catalog. Three built-in sources: AM database (`programs/x86_64-appimages`, `◆ package : description` lines), AppImage Hub feed (`appimage.github.io/feed.json`, 1,383 curated apps with icons, categories, authors) and pkgforge (`meta.pkgforge.dev/pkgcache/x86_64-Linux.json`, redirects to GitHub; a JSON array, `parsePkgforgeJsonSync()` keeps `pkg_type` appimage that is installable, not `_disabled`/`deprecated` (string flags), one build per `pkg_name` preferring non-prerelease `pkg_id`; about 86 apps). Defaults (new installs, Reset Catalogs): pkgforge on, AppImage Hub and AM off; existing catalog lists get pkgforge appended once (`Store/addedPkgforge` in QSettings). `effectiveUrl()` points a stored pkgforge/AM list address at this CPU's list (`catalogArch()`: x86_64 or aarch64; other CPUs: empty = not loaded, `available: false`). `sources()` carries `note` (AppImage Hub's reliability, AM needing `am`/`appman`, unavailable CPU) shown in Settings and the welcome page. `amInstalled` (looked up once) drives the "Needs AM" button; `amScriptBase` gives the CPU's script folder. pkgforge entries copied from upstream ("Fetched from Upstream" note, GitHub/Codeberg `src_url`) carry `repoUrl`, which `startDownload()` saves as the update source. Source types: `appimagehub`, `am-database`, `universal`, `pkgforge`. Both fetched in parallel; **parsing runs on worker threads** via `QtConcurrent::run` + `QFutureWatcher<QVector<StoreApp>>`; results marshalled back via `onAmParsed`/`onFeedParsed`. Disk cache in `QStandardPaths::CacheLocation` with 24h freshness check. `checkAllLoaded()` merges sources on a worker thread; Theme icon names (AM entries use the package name) are not checked against the theme; the rows' `Kirigami.Icon` has `fallback: "application-x-executable"` (see the pitfall). `applyFilter()` dispatched to worker with `m_filterGeneration` counter to discard stale results; without a category filter it sorts by first category (empty last) then name, so the list can show category sections (`sectionCategory` role). `storeSource` (0 = all, otherwise the 1-based position in `sources`) persisted in `QSettings`; `setSourcesOrder()`/`removeSource()` move it with its catalog. SVG icons filtered at parse time — Qt's renderer crashes on CSS mesh-gradient SVGs. **Lazy-loaded: never call `load()` from constructor**; call `initialize()` once from QML when Discover page first becomes visible (and `AppSettings.discoverEnabled` is on). Source mutators (`addSource`/`updateSource`/`removeSource`/`resetSourcesToDefault`) only `sync()` once initialized, so toggling sources in onboarding/settings causes no network traffic before Discover is opened |

### D-Bus (`src/dbus/`)

| Class | Files | Role |
|-------|-------|------|
| `AppImageDBusAdaptor` | `appimagedbusadaptor.h/.cpp` | `QDBusAbstractAdaptor` mounted on object path `/io/github/appimagemanager/Manager`, interface `io.github.appimagemanager.Manager1`. Read-only: `InstalledAppImages()` returns `QVariantList` of `{path, name, version, hasDesktopLink}` maps; `Version()` returns project version string |

---

## QML File Map

All QML lives in [qml/](qml/) (top-level, not under `src/`). Files are wired into `appimagemanager_qml` via `qt_add_qml_module` in [src/CMakeLists.txt](src/CMakeLists.txt).

| File | Type | Context / Properties |
|------|------|----------------------|
| `ManageWindow.qml` | `ApplicationWindow` (fixed 26×22 gridUnits) | Context prop: `backend` (AppImageBackend). Drag-and-drop install — floating `dragIcon` surrogate used as drag target, original icon fades to 30% opacity during drag. No box or chips: name, one dim `version · size` line, a small line when another copy is installed ("Version X is installed · this one is newer/older"), icons at `iconSizes.huge`. Own title "<App> – AppImage Manager" and constant min/max size (window-size restoring scripts matched it to the dashboard). On drop: `replaceDialog` (Replace / Keep Both (hidden for the same file name) / Cancel) when `existingState` is set, then `backend.beginInstall(replace)` (not `installAppImage`). Keyboard: Ctrl+Shift+→ runs `keyboardMove` (slides the drag surrogate onto the folder with `Drag.active` on, so the drop highlight reacts) and then the same `requestInstall()` as a drop; Enter launches once installed (both off while a dialog is open); `onSignatureCheckReady` handler routes to `doInstall()` or opens `signatureWarnDialog`. Inline `UninstallDialog` and `signatureWarnDialog` (`Kirigami.Dialog` with security icon, Ok/Cancel) |
| `DashboardWindow.qml` | `Kirigami.ApplicationWindow` | Context props: `listModel`, `proxyModel`, `dashboardController`, `amStoreModel`. A `Controls.ToolBar` on top of a `StackLayout` of 4 pages: Installed (`LibraryPage`, 0, default), Discover (`StorePage`, 1), Settings (2), About (3). Toolbar: checkable Installed ("Installed (N)" while `listModel.updateCount > 0`) / Discover tabs, Update All (Installed tab, `updateCount > 1`), ⟳ Check for Updates (Installed tab, icon only, disabled while checking), `+` Install from File, main menu (Check for Updates, Settings, About). Settings/About act as pushed pages: the tabs give way to Back + title; `goBack()` returns to `lastTab`; Alt+Left goes back, Ctrl+, opens Settings. Global drag-and-drop overlay for `.AppImage` files |
| `UpdateSourceDialog.qml` | `Kirigami.Dialog` | Per-AppImage update source: one field, live "detected as" line from `listModel.describeUpdateSourceInput()`, a Type menu to force the kind, pre-releases only for repositories. Opened via `openFor(proxyRow, name)` from the LibraryPage detail `⋯` menu. Cancel is a standard button; Save and Use Built-in are `customFooterActions`. Repo field (prefilled with the override or the embedded GitHub repo), "Include pre-releases", "Use Built-in" (clears the override). Saving the unchanged embedded repo without pre-releases stores no override. Signal `saved(cleared)` |
| `ConfigFolderDialog.qml` | `Kirigami.Dialog` | Settings folder on/off for one app (`openFor(proxyRow, name)` from the ⋯ menu "Keep Settings in “Applications”"): turning on lists its `~/.config` folders (checked when matched by app id), off lists the folder's contents to move back |
| `ReleaseNotesDialog.qml` | `Kirigami.Dialog` | ⋯ → Release Notes… (only with a repository source): `openFor(proxyRow, itemData)` fetches, BusyIndicator, then each release as Heading + dim tag · date · Pre-release + `Text.MarkdownText` body (links open externally); PlaceholderMessage on failure; footer "Open on GitHub" (newest release page). Fixed `preferredHeight` (Dialog binding-loop pitfall); the Dialog's own ScrollView scrolls |
| `SandboxDialog.qml` | `Kirigami.Dialog` | Sandbox on/off and the folders the app may use (`FolderDialog` to add, remove per row); warning and disabled switch without bwrap. From the ⋯ menu "Sandbox…" |
| `EnvironmentDialog.qml` | `Kirigami.Dialog` | Per-AppImage environment: `openFor(proxyRow, name)` from the LibraryPage `⋯` menu; monospace `TextArea`, one `KEY=value` per line; `proxyModel.setEnvironment()` returns invalid line numbers, shown in an `InlineMessage`. Signal `saved()` |
| `LibraryPage.qml` | `Kirigami.Page` | Context props: `listModel`, `proxyModel`. Master-detail layout. Left pane (width draggable by a `resizeHandle` over the separator: `leftPane.dragWidth` while dragging, saved to `AppSettings.libraryListWidth`, double-click = default 16 gridUnits, clamped so the details keep 20): search + Sort button (menu), thin progress bar (scanning or checking), error `InlineMessage` with Retry for failed update checks (progress and the result are `UpdateCheckToast`), list pane is a `Rectangle` with the View color set (panel); rows = icon, name and a status line (`rowStatus()`: "0.18.1 · Up to date" dim, "1.17.0 → 5.14.0" positive, "· No update source" dimmer, "· Check failed" with a warning icon and the reason as tooltip, "· Older copy of X"), a progress bar while updating; `Kirigami.ListSectionHeader` sections from the `listSection` role (Binding sets `listModel.sectionMode` from the sort; empty section = no header); a dim status-filter line with a clear button; while nothing is selected and `AppSettings.showOverview`, the right side is the overview (count, total size, one clickable line per state that sets `proxyModel.statusFilter`, last check, Check Now, Update All); optional drop box at the end of the overview (`AppSettings.showInstallBox`; outlined, as wide as the overview, 0.75 × its width tall, minimum 6 gridUnits; with the "unverified" reminder under it; `dropBoxShown` tells `DashboardWindow`'s drop overlay to stay out of the way). Right pane (width capped at 38 gridUnits for readable lines, horizontally centered in the pane like the overview): icon + level-1 name + dim developer · homepage, actions row (Launch, "Update to X" or a progress bar while updating, `⋯` menu: Show in App Menu, Update Source…, Environment Variables…, Remove…), "About" section (description) and "Details" key/value grid (Version, Size, Category (readable), Added, Updates = source + what the last check said, or "Set Update Source…" without one, App menu Shown/Not shown, Environment, File). `displayedItem` lags `currentItem` (80ms fade-out → swap → 150ms fade-in); data updates sync immediately. `updateAll()` starts every available update via the source model. Hosts `UpdateSourceDialog` and `EnvironmentDialog`. Shortcuts (only while visible): Return=Launch, Delete=Remove, Escape=clear search / deselect |
| `StorePage.qml` | `Kirigami.Page` | The Browse tab (called Discover before 2.3; code names `StorePage`, `discoverEnabled`, `pageDiscover` kept), 1:1 with `LibraryPage`. Context prop: `amStoreModel`. Left pane: same width as Installed (`AppSettings.libraryListWidth`, same `resizeHandle`), search + Sort button (sort icon; the menu holds Sort A–Z/Z–A, Source submenu, Category submenu); active filters as a dim line with a clear button; thin progress bar while loading; list pane is a View-colored panel; rows = icon + name + small dim author line (drawn like Installed's status line), generic icon when none (`fallback`), grouped under `sectionCategory` headers unless a category is filtered. A model reset (filter change) clears the selection. Right pane: same layout as LibraryPage (centered, capped at 38 gridUnits, level-1 name, dim author · website host link). Primary action "Install" (AM) or "Download" (Hub); `⋯` offers the other source when both exist, Open Website, View Install Script. While installing: progress bar (stage 0–5) + Cancel, a dim status line (last log line) and "Show Details" for the plain monospace log, which opens by itself on failure. Then "About" (description) and "Details" (Category, License, Available from: each catalog with install script / download, Links: Website, AM install script). The AM script URL is never shown as the website. Shortcuts (only while visible): Return = primary install, Escape = clear search / deselect. Lazy init: `initializeIfNeeded()` (on visible change and on `discoverEnabledChanged`) calls `amStoreModel.initialize()` once, only when `AppSettings.discoverEnabled`. When off, a `PlaceholderMessage` with "Turn On Discover" replaces the page |
| `SettingsPage.qml` | `Kirigami.Page` | `AppSettings` singleton. Laid out after Sirkel's settings (`comp/LoopLinux/src/kcm/ui`): a View-colored sidebar `ListView` of pages (General, Updates, Security, Discover) with Restore Defaults under it, next to a `StackLayout` of `FormPage`s (inline component: centered column capped at 30 gridUnits) holding a `Kirigami.FormLayout` ("Label:" + control, `isSection` separators) and `Hint` labels (dim small text under a control). Discover also holds the catalogs as a framed list (drag to reorder, switch, edit, delete) with Add Catalog… / Reset Catalogs. No FormCard here |
| `UpdateCheckToast.qml` | `Item` | Bottom-center toast in `DashboardWindow` (styled like Kirigami's passive notifications: Complementary `ShadowedRectangle`): "Checking for updates (x/y)" from `listModel.checkDone/checkTotal` while checking, then the result for 4 s. Failed checks are left to LibraryPage's InlineMessage with Retry |
| `AboutContent.qml` | `ColumnLayout` | The About content (app info, license, links, library versions, contributors via a `ContributorsModel` instance). Shared by `AboutPage` and `AboutDialog`; call `fetchContributors()` when shown |
| `AboutPage.qml` | `Kirigami.Page` | ScrollView around `AboutContent` (dashboard sub-page from the main menu) |
| `AboutDialog.qml` | `Kirigami.Dialog` | ScrollView around `AboutContent`, for `ManageWindow` |
| `UninstallDialog.qml` | `Kirigami.Dialog` | Prop: `backend`. Corpse checklist + total size + confirm. `Kirigami.InlineMessage` shows KIO trash errors. Sets `backend = null` on close |
| `OnboardingDialog.qml` | `Kirigami.Dialog` | Single page shown on `AppSettings.firstRun == true`: Applications folder, Downloads notifications, Enable Discover + per-catalog checkboxes from `amStoreModel.sources`, Dolphin tip, "Get Started". `onOpened` sets `discoverEnabled = false` on first run, making Discover opt-in for new installs; `onClosed` clears `firstRun`. `closePolicy: NoAutoClose`, no close button. `parentWindowWidth` property required — QML file scope does not inherit outer document IDs, so `root.width` is inaccessible; the width is passed explicitly from `DashboardWindow.qml`. `Layout.preferredWidth: 0` on all long labels prevents them from driving the dialog beyond `preferredWidth` (Qt layouts use `implicitWidth` to compute parent `implicitWidth`; wrapping text still reports its unwrapped natural width) |
| `TrayWindow.qml` | `Window` (frameless) | The tray icon's window, loaded by `UpdateDaemon`. Context props: `trayModel` (change signals), `daemon` (reads `daemon.State()` JSON, like the tray widget: updates first, "Not tracked (n)" fold). One-line header in the small font (status, then dim `· lastCheckText`; the next check is only in the tooltip), list of AppImages with their state (row click → `daemon.openManager(path)`), Check Now, Update All, Open, Settings. Hides on focus loss and Esc. Overview only: managing happens in the main window |
| `ContributorsModel.qml` | QtObject (instantiated, not a singleton) | Contributor data and fetch logic (XHR to GitHub API, bot-filtered, fallback list). Instantiated once inside `AboutContent.qml` |

---

## Data Flows

### Manage window: open → install

```
AppImageWindow::open(path)
  └─ AppImageBackend(path)            starts QtConcurrent::run
       └─ AppImageReader::read()      BLOCKING, worker thread
            ├─ libappimage path: in-process SquashFS (preferred)
            └─ squashfuse path: subprocess mount → parse → fusermount3 unmount
       └─ onMetadataReady(info)       main thread; emits metadataLoadedChanged
  └─ ManageWindow.qml                 BusyIndicator until metadataLoaded; then icon + info shown
  └─ user drags icon to folder
       └─ backend.beginInstall()
            ├─ if verifySignatures OFF → doInstall() immediately
            └─ if verifySignatures ON  → QtConcurrent::run(AppImageReader::verifySignature)
                 └─ QFutureWatcher::finished → signatureCheckReady(state)
                      ├─ state Valid/Unchecked/GpgUnavailable → doInstall()
                      └─ state Unsigned/Invalid → signatureWarnDialog.open()
                           └─ user clicks OK → doInstall()
       └─ doInstall() → AppImageManager::installAppImage()
                 ├─ KIO::move  if source under ~/Downloads
                 └─ KIO::copy  otherwise (preserves USB / project sources) + chmod +x on result
            └─ onInstallJobFinished → isInstalled = true → folder column fades → icon centers
```

### Dashboard: initial scan + live watching

```
DashboardWindow::setupAndShow()
  └─ AppImageListModel::scan()
       └─ QDir(applicationsPath).entryInfoList(*.AppImage / *.appimage)
       └─ main-thread cache pre-check; skip worker spawn on warm cache hit
       └─ otherwise QtConcurrent::run(AppImageReader::read())
            └─ QFutureWatcher::finished
                 → if (gen != m_generation) drop the result
                 → applyMetadata(row, info); dataChanged(row, row)
                 → if (--m_pendingLoads == 0) scanning = false
  └─ KDirWatch (WatchFiles) on applicationsPath
       ├─ created(path)  → onFileCreated  (begin/endInsertRows + loadMetadataForRow)
       ├─ deleted(path)  → onFileDeleted  (begin/endRemoveRows)
       └─ dirty(path)    → onFileDirty    (re-read metadata for one row)
  └─ DownloadWatcher (when watchDownloads is on; suppressed if daemon D-Bus name is registered)
       └─ appImageFound(path, displayName) → KNotification "Manage" → ManageWindow
```

### Update check + download

```
listModel.checkForUpdates()
  └─ AppImageUpdateManager::checkForUpdates({CheckItem ...})
       └─ per item: updateInfo = effectiveUpdateInfo(embedded, loadUpdateSource(path))   [user's source wins]
            ├─ gh-releases-zsync / forgejo-releases → ReleaseChecker (Releases API; list when pre-releases allowed)
            │                      → pickAsset(): .zsync, else arch-matching .AppImage
            ├─ direct|url        → ReleaseChecker HEAD: Last-Modified/ETag vs installedTag (or file mtime)
            └─ plain zsync| URL  → fetch .zsync header, parse SHA-1, hash local file off-thread
       └─ updateFound + itemChecked(path, result, reason) → row state → "Updates available" section, "Update to X", "Installed (N)"
       └─ checkFinished → results written to checkresults, daemon told (ReloadState)
  └─ user clicks Update to X → proxyModel.downloadUpdate(row)   (Update All loops over listModel rows)
       └─ AppImageUpdateManager::downloadUpdate(...)
            ├─ .AppImage URL          → direct HTTP download to old.new (release without .zsync, direct source); web page → refused
            └─ .zsync URL            → ZsyncDownloader: local blocks + HTTP range requests → old.new, SHA-1
                                         (failure → full HTTP download of the URL: from the .zsync)
            └─ on success: rename old→old.bak, rename old.new→old, bak → .previous/<name> (keepPrevious; removed if that fails), chmod +x, downloadFinished(path, true)
                           (on rename failure: restore bak, remove new, emit failure)
            └─ release-type source: saveInstalledTag(path, version) so version-less AppImages stop re-offering it
```

### Discover page: store catalog load

```
StorePage.qml initializeIfNeeded() (first open with AppSettings.discoverEnabled)
  └─ amStoreModel.initialize()
       └─ AMStoreModel::load(forceNetwork=false)
            ├─ ~/.cache/AppImageManager/am-database.txt  fresh? → read file
            │   OR GET https://raw.githubusercontent.com/ivan-hc/AM/main/programs/x86_64-appimages
            │        → save cache
            └─ ~/.cache/AppImageManager/am-feed.json     fresh? → read file
                OR GET https://appimage.github.io/feed.json
                     → save cache
       (two QtConcurrent::run workers in parallel)
       ├─ parseAmDatabaseSync(rawText)  → QVector<StoreApp>  [worker thread]
       └─ parseFeedJsonSync(data)       → QVector<StoreApp>  [worker thread]
            └─ PNG/JPG/WEBP icons only — SVG paths skipped (crash guard)
  └─ onAmParsed / onFeedParsed  [main thread via QFutureWatcher::finished]
       └─ checkBothLoaded()
            └─ mergeDatabases()   [worker thread; no theme icon lookups]
                 └─ cross-ref by normalizedName; Hub metadata wins on description/categories
            └─ applyFilter()  → QtConcurrent::run worker (filter + localeAwareCompare sort)
                 └─ m_filterGeneration discards stale results if filter changed mid-flight
                 └─ beginResetModel / endResetModel on main thread when done
  └─ amStoreModel.loading = false → BusyIndicator hides → list appears
```

### Uninstall flow

```
user clicks Remove
  └─ backend.findCorpses()           async QtConcurrent::run
       └─ AppImageManager::findCorpses(info)
            scans ~/.config, ~/.local/share, ~/.cache for dirs matching appName/appId
            filters via ~60-entry blacklist (plasma, kwin, python, git, etc.)
       └─ CorpseModel::setCorpses()  populates model; UninstallDialog shows checkboxes
  └─ user selects items; confirms
  └─ backend.removeAppImageAndCorpses(paths)
       └─ AppImageManager::removeItems()  → KIO::trash() (never permanent delete)
       └─ AppImageManager::removeDesktopLink() if exists
       └─ emits uninstallFinished → ManageWindow closes / KDirWatch fires removal into dashboard
```

---

## Key Conventions

### Threading
- `AppImageReader::read()` and `AppImageManager::findCorpses()` are **always blocking** — call only via `QtConcurrent::run`.
- `AppImageListModel` dispatches reader workers through `m_readerPool` (private `QThreadPool`, cap `qBound(1, idealThreadCount / 2, 4)`) — not the global pool — to prevent thread storms on large libraries.
- `AppImageUpdateManager` hashes local AppImages off-thread via `QtConcurrent::run`.
- All async results marshal back to main thread via `QFutureWatcher::finished`.
- Never touch `m_items`, `m_scanning`, or `m_generation` from worker threads.

### Deletion
- User files go to `KIO::trash()` — **never** `QFile::remove()` or `QDir::removeRecursively()`.
- Connect `KJob::result()` before calling `job->start()`.

### Backend lifetimes
- `AppImageWindow` owns one `AppImageBackend` for its entire lifetime.
- `DashboardWindow::m_uninstallBackend`: use `deleteLater()` when replacing, never raw `delete` — in-flight `QFutureWatcher` lambdas capture `this`.
- `AppImageIconProvider` is parented to the backend (`iconProvider->setParent(backend)`) so it outlives engine teardown. Pass pointer to engine via `addImageProvider()`.

### Desktop file paths
- `AppImageManager::desktopFilePath(info)` → `~/.local/share/applications/<appId>.desktop` when `appId` is reverse-DNS and no system package has that id, else `appimage_<fileKey>.desktop` (the pre-2.1.1 name)
- `AppImageManager::iconFilePath(info)` → `~/.local/share/icons/hicolor/<size>/apps/appimage_<fileKey>.<ext>`
- `fileKey` = clean-name slug + hash of the file name (see `AppImageManager`).

### QML patterns
- All windows: `Kirigami.Theme.colorSet: Kirigami.Theme.Window` + `Kirigami.Theme.inherit: false`.
- Animated show/hide: `visible: opacity > 0` + `Behavior on opacity { NumberAnimation }`.
- Dialogs: take a `backend` (or domain-specific) property, reset it to `null`/defaults in `onClosed`.
- `UninstallDialog` / `UpdateSourceDialog` are dialogs, not windows — instantiated inline in their parent.
- Keep it quiet: stock Kirigami controls and theme colors only (no custom borders or palettes), dim secondary text via `Kirigami.Theme.disabledTextColor`, one metadata line instead of chips, a primary action plus a `⋯` menu, passive notifications or `InlineMessage`s instead of modal dialogs (modals only for destructive actions).
- Use `listModel.itemData(row)` for bulk role reads; per-role `.data()` round-trips dominate the detail pane otherwise.

### Shared helpers (`appimageinfo.h`)

Four `inline` utilities defined at file scope in `appimageinfo.h` (included by everything):

- `normalizeVersion(v)` — strips leading `v`/`V` from version strings.
- `isNewerVersion(remote, local)` — compares dot-split version tuples; returns true if remote > local.
- `kAppImageFilters()` — returns `{ "*.AppImage", "*.appimage" }` as a static `QStringList`. Use everywhere instead of inline literals.
- `isRunningInSandbox()` — returns `true` when `FLATPAK_ID` or `SNAP` env var is set. Result is cached in a `static const bool` evaluated once at program start. Used by `DownloadWatcher` to skip KDirWatch setup and by QML via `listModel.downloadWatcherSandboxed`. No extra include needed — `qgetenv` is covered by `<QByteArray>`.

### D-Bus
- **Daemon name** `io.github.appimagemanager.Daemon` — registered by `UpdateDaemon` on start. `AppImageListModel`'s download watcher path checks this via `QDBusConnection::sessionBus().interface()->isServiceRegistered(...)` and silently returns if the daemon is running — prevents duplicate download notifications.
- **Read-only adaptor** `io.github.appimagemanager.Manager1` on `/io/github/appimagemanager/Manager` (mounted by the dashboard process) — exposes `InstalledAppImages()` and `Version()` for external integrations.

### AppImage type support
- **Type 2** (SquashFS, ELF+magic 0x414902): primary target, full support.
- **Type 1** (ISO9660): detected, best-effort only — not a priority, may have extraction gaps.

### i18n
- QML: `i18n("string")` / `i18n("string %1", value)`. Enabled by `KLocalization::setupLocalizedContext(engine)`.
- C++: `i18n()` from `<KLocalizedString>`.
- New source files with strings go into `po/Messages.sh`.
- English only for now: the 15 machine-drafted languages (all fuzzy, so never shown) live in `po-drafts/`, outside `ki18n_install(po)`. A reviewed language moves back to `po/<lang>/`.
- `.pot` regenerated by CMake (`ki18n_install(po)`). Add language: copy `po/appimagemanager.pot` → `po/<lang>/appimagemanager.po`.

---

## Known Pitfalls

| Issue | Detail |
|-------|--------|
| **Clangd false positives** | `appimagemanager_qml_export.h` is generated into `build/dev/src/`. Clangd reports cascade errors on all GUI files unless `compile_commands.json` is indexed. Dev preset has `CMAKE_EXPORT_COMPILE_COMMANDS=ON` — run `cmake --preset dev` once to generate it. All `unknown_typename` errors on Qt/KDE macros are false positives if the build succeeds |
| **`Kirigami.Units.fonts` missing** | Does not exist in Kirigami 6. Use `Kirigami.Theme.smallFont` (a `font` object) |
| **`anchors.fill` + `drag.target`** | Anchors override x/y set by drag system. Items that are drag targets must use explicit `width`/`height`, not `anchors.fill` |
| **`AnchorChanges` in States** | Does not support `anchors.fill` shorthand. Only individual sides: `anchors.left`, `anchors.top`, etc. |
| **`clip: true` on rounded Rectangle** | Clips to bounding rect, ignores `radius`. Cannot achieve rounded clip this way |
| **`beginFilterChange`/`endFilterChange`** | Available since Qt 6.9 (now the project minimum). Prefer the new API for filter mutations that need begin/end semantics; `invalidateFilter()` still works for the common case |
| **`CorpseModel` QML_ELEMENT** | Do not add `QML_ELEMENT` — triggers Qt 6.11 constexpr metaobject `static_assert` |
| **KDBusService mode** | Bifurcated by invocation: file/daemon modes use `Multiple` (Dolphin can open several manage windows in parallel; the daemon registers its own well-known D-Bus name). Dashboard mode (`appimagemanager` / `--dashboard`) uses `Unique`; a second invocation pings `activateRequested` and raises the existing window via `DashboardWindow::open()` |
| **`AppImageCache` version** | Cache is versioned (`kCacheVersion = 5`, SQLite-backed; 5 re-reads every AppImage once after the appId fix). Adding new fields to `AppImageInfo` that must be persisted requires bumping `kCacheVersion`, adding a column to the `CREATE TABLE metadata` schema, and adding the field to both `load()` and `store()`. Forgetting any of the three silently returns empty strings for the new field |
| **`KDirWatch` over `QFileSystemWatcher`** | Both `AppImageListModel` and `DownloadWatcher` use `KDirWatch` in `WatchFiles` mode. It delivers per-file `created(path)` / `deleted(path)` / `dirty(path)` (deduplicated internally), so the model does incremental inserts/removes instead of debouncing + full re-scans. Pre-existing files do NOT replay on `addDir`, so seeding by hand is unnecessary. `dirty(path)` fires on the watched dir itself when a child is added or removed — handlers must `if (path == m_watchedDir) return;` to avoid double-handling |
| **`m_generation` invalidation** | `AppImageListModel::refresh()` increments `m_generation`; in-flight metadata futures capture the generation they started in and drop their result if the model has moved on. Always check `gen != m_generation` before mutating `m_items` from a future continuation |
| **Built-in zsync** | `ZsyncDownloader` (`src/gui/zsyncdownloader.h`) replaces `zsync2`: weak rolling checksum (last `rsum_bytes` of a/b, big-endian) + truncated MD4 per zero-padded block, `seq_matches` 2 needs the next block too, with `seq_matches` 2 the next window's weak sum is rolled too and must match before any MD4 (else short weak sums make most positions candidates: minutes on a changed 30 MB file), missing blocks fetched with single-range requests (4 at a time, gaps up to 64 KiB bridged; more than 300 requests or 75% of the file → full download instead), whole result SHA-1 checked. A server answering 200 instead of 206 → full download. `tst_zsync live` runs against a real server when `AIM_ZSYNC_URL`/`AIM_ZSYNC_LOCAL` are set |
| **`QLatin1String` and multi-byte UTF-8** | `QLatin1String("• ")` misinterprets the bullet (U+2022, 3 UTF-8 bytes) as 3 Latin-1 chars. Always use `QStringLiteral("• ")` for non-ASCII string literals |
| **Kirigami Dialog `preferredHeight` binding loop** | Binding `preferredHeight` to `dialog.window.height` (e.g. `preferredHeight: dialog.window.height - someOffset`) creates a circular dependency: Kirigami computes the dialog's `y` from `window.height` + its own `height`, which depends on `preferredHeight`. Result: `Binding loop detected for property "y"` at runtime, followed by SIGABRT via `free(): invalid size`. Use a fixed `Kirigami.Units.gridUnit * N` value instead, or a property that does not involve the dialog's own position or size |
| **`pendingLoads` dispatch ordering** | In `AppImageListModel::refresh()`, `m_pendingLoads` must be assigned **before** any `loadMetadataForRow()` calls. The cache fast-path decrements `m_pendingLoads` synchronously on the main thread; if the assignment comes after, those decrements race against the final value and leave a stuck non-zero count ("Loading N…" never clears). Always: compute the expected count → assign → emit → dispatch |
| **Plasma `ItemDelegate` position-aware insets** | The Plasma/Kirigami QQC2 style applies a larger `bottomInset` to the last item in a `ListView`, making its hover/selection highlight appear vertically squashed. Fix: set `topInset`, `bottomInset`, `leftInset`, `rightInset`, and `verticalPadding` explicitly on the delegate so all rows use identical geometry |
| **`AMStoreModel` lazy init** | Never call `load()` from the constructor — the AM database + feed.json are ~500 KB of network + JSON parsing that blocks the main thread. Call `amStoreModel.initialize()` from `StorePage.qml`'s `onVisibleChanged` (guarded by `!storeInitialized`). `initialize()` is idempotent; `sync()` forces a re-fetch regardless |
| **`AMStoreModel` SVG crash** | `parseFeedJsonSync()` skips any icon whose path does not end in `.png`/`.jpg`/`.jpeg`/`.webp`. Feed.json contains SVGs that use CSS mesh gradients — Qt's SVG renderer `free(): invalid size` → SIGABRT when it encounters them. Never remove this extension filter |
| **`AMStoreModel` icon base URL** | Icon paths in `feed.json` are relative paths like `86Box/icons/512x512/86box.png`. The correct base is `https://appimage.github.io/database/` — producing `https://appimage.github.io/database/86Box/icons/512x512/86box.png`. Using `https://appimage.github.io/` (without `database/`) returns 404 for all icons |
| **`OnboardingDialog` sizing** | `preferredHeight` must be a fixed `gridUnit * N` — binding it to `root.height` causes the Kirigami Dialog `y` binding loop → SIGABRT (see **Kirigami Dialog `preferredHeight` binding loop** pitfall). For `preferredWidth`, `root` inside `OnboardingDialog.qml` does NOT resolve to the outer `DashboardWindow.qml`'s `id: root` — QML file scope does not inherit outer document IDs. Pass window width via an explicit property: `parentWindowWidth: root.width` from the instantiating file. Additionally, `Controls.Label` with `wrapMode: Text.WordWrap` reports its natural (unwrapped) line width as `implicitWidth`; Qt Layouts use `implicitWidth` to compute parent `implicitWidth`, which makes `Kirigami.Dialog` expand beyond `preferredWidth`. Fix: set `Layout.preferredWidth: 0` on all long labels |
| **`AMStoreModel` worker threading rules** | `parseAmDatabaseSync()` and `parseFeedJsonSync()` are `static` pure functions. **Never call `QIcon::hasThemeIcon()` per catalog entry**: ~10,000 lookups took 34 s (and KIconTheme creates objects on the worker thread). Leave missing names to the delegate's `Kirigami.Icon` `fallback`, which only looks up visible rows. Cross-source lookups in `checkAllLoaded()` go through hashes: a per-app scan of another source took 8.5 s. `applyFilter()` dispatches to a worker via `QtConcurrent::run`; uses `m_filterGeneration` (incremented on each call) so the `QFutureWatcher::finished` lambda silently discards stale results |
| **`Kirigami.Page` actions outside a PageRow** | The dashboard pages live in a `StackLayout`, not `pageStack`, so a page's `actions`, `title` and global toolbar are never rendered. Sort and Restore Defaults were unreachable this way. Put controls in the page content or in `DashboardWindow`'s toolbar |
| **Lists go inside `Controls.ScrollView`** | A bare `ListView` with an attached `ScrollBar` lays the scrollbar over the rows (selection highlights run under it) and scrolls slowly with the mouse wheel. The desktop-style `Controls.ScrollView` pads the content for the scrollbar and installs `Kirigami.WheelHandler`. Put the sizing (`anchors`/`Layout.*`) on the ScrollView, not the ListView |
| **QML singleton factory takes ownership** | A `QML_SINGLETON` `create()` that returns an existing object must set `QJSEngine::CppOwnership` first, or the engine deletes it on destruction. `AppSettings` is a function-local static: closing the dashboard aborted with `free(): invalid size` |
| **Settings live in two processes** | The dashboard writes `appimagemanagerrc`; the daemon is a separate process whose `AppImageManagerSettings::self()` caches values, and `AppSettings` change signals only fire in the process that called the setter. The daemon re-reads with `AppImageManagerSettings::self()->load()` on its tick; shared runtime data (check results) goes through `checkresults` + D-Bus notifications |
| **D-Bus service files: `[D-BUS Service]`** | dbus-broker (Arch's default) only accepts the section name exactly as `[D-BUS Service]`; `[D-Bus Service]` is rejected ("Missing 'D-Bus Service' section"), so D-Bus activation silently never happens |
| **Plasma `SignalWatcher` handler names** | `org.kde.plasma.workspace.dbus` `SignalWatcher` calls a QML function named `dbus` + the D-Bus signal name (`StateChanged` → `function dbusStateChanged()`). A function named like the signal itself is not called, and QML function names cannot start with a capital letter |
| **Delegate property named after a context property** | `property int index: index` in a delegate binds the property to itself (always 0), not to the model's `index`; the Settings catalog drag-reorder was dead because of it. Use another name (`readonly property int itemIndex: index`) |
| **Remote icons in long lists** | `Kirigami.Icon` with an http(s) source decodes on the UI thread; Discover rows use `Image { asynchronous: true; sourceSize: 2 × icon size }` for remote icons and `Kirigami.Icon` only for theme names |
| **Detail pane `displayedItem` pattern** | `LibraryPage` and `StorePage` both use a `displayedItem` property that lags behind `currentItem`. On selection change: `currentItem` updates immediately (so action-bar logic stays correct), then `SequentialAnimation` fades `detailInner` to 0 → ScriptAction updates `displayedItem` → fades back to 1. Data-only updates (metadata loaded, update check result) bypass the animation and sync `displayedItem` directly to avoid flickering on in-place updates |
| **`installAppImage()` removed from `AppImageBackend`** | The old single-step `installAppImage()` slot is gone. Use `beginInstall()` → wait for `signatureCheckReady(state)` → call `doInstall()`. QML in ManageWindow already does this via the `Connections` handler. Any test or external code that called `backend.installAppImage()` directly must be updated |
| **`appId` comes from the inner `.desktop` file name** | `AppImageReader` uses `QFileInfo::completeBaseName()` (`org.foo.Bar.desktop` → `org.foo.Bar`). `baseName()` stops at the first dot and turned ids into `org`/`io`/`ai`; `computeIconSource()` then preferred an unrelated theme icon of that name over the AppImage's own icon. Fixed with cache version 5 |
| **`fileKey()` hash suffix is a 2.0 breaking change** | Apps now get a `_<hash>` suffix in their `.desktop` and icon filenames. Pre-2.0 desktop links (without the suffix) will no longer be detected by `isDesktopLinkEnabled()`. Users must re-toggle those links once after upgrading. Do not add a migration fallback that checks the legacy path — it would mask the slug collision bug the change was made to fix |
| **Update sources and environment are keyed by file name** | `[UpdateSources][<file name>]` and `[Environment][<file name>]` in `appimagemanagerrc`, so a moved Applications folder keeps them (updates keep the path), but renaming an AppImage loses them. Both loaders call `reparseConfiguration()` because the dashboard, manage windows and daemon are separate processes. Saving an environment rewrites an existing app menu entry so its `Exec=` matches |
| **`discoverEnabled` defaults to `true`** | Deliberate: existing installs keep Discover without a migration. New installs get opt-in behaviour because `OnboardingDialog.onOpened` sets it to `false` on first run. "Restore Defaults" turns it back on |
| **No PKGBUILD in the repo root** | makepkg uses `<PKGBUILD dir>/src` as `$srcdir`; in the root that is the project's `src/` (and `makepkg -C` deletes it). The local PKGBUILD lives in `packaging/` and builds `$startdir/..` |
| **Apps write their own menu entries** | Some AppImages (and vendor launchers such as ArtCraft Launcher) write `~/.local/share/applications/<their id>.desktop` themselves. Never assume our file is the only entry: detect by `Exec` target, name ours after the app's id, and only ever delete our own `appimage_*` files automatically |
| **Translated desktop keys and KConfig** | `writeEntry("Name[de]", …)` escapes the brackets (`Name\x5bde\x5d`), and `setLocale()` + `KConfig::Localized` wrote one language's text under another's key. `createDesktopLink()` writes its own keys with KDesktopFile and then inserts the app's lines verbatim under their group headers. Also: `KDesktopFile` with a bare file name resolves into `~/.local/share/applications`, so always pass absolute paths (a scratch test once left an `out.desktop` there) |
| **GitHub counts unauthenticated 304s** | Conditional requests (`If-None-Match`) only save rate limit when authenticated; without a token a `304 Not Modified` still uses one of the 60 requests per hour (measured). The public `releases.atom` feed carries no rate-limit headers, hence feed first |
| **Catalog "Download" links are rarely files** | Of AppImage Hub's 3,792 Download links, 3,735 are GitHub releases pages, 31 other web pages and 26 actual `.AppImage` files (2026-11). `AMStoreModel::resolveAndDownload()` turns a GitHub (or `/releases` link on another host: Forgejo API) repository link into the latest release's arch-matching `.AppImage` (`ReleaseChecker::matchesArchitecture`/`pickAsset`), `startDownload()` aborts on a web-page Content-Type and refuses a file without the ELF + `AI` mark, and the repository becomes the update source when the AppImage has none. Before this, web pages were saved as `<app>.AppImage` |
| **DwarFS AppImages** | Newer runtimes (uruntime) can pack the payload as DwarFS instead of SquashFS (`DWARFS` header after the ELF; still `AI\x02` at offset 8). libappimage only reads SquashFS, so such apps run but show no name/icon/update info ("This doesn't look like a squashfs image") |
| **AppImage thumbnails already exist** | KDE's `kio-extras` ships `appimagethumbnail` (Dolphin preview plugin for `application/vnd.appimage`); don't write our own |
| **`verifySignature()` requires `gpg` or `gpg2` on PATH** | If neither is found, `verifySignature()` returns `GpgUnavailable`. The QML treats this like `Valid` (proceeds without dialog) so users without gpg installed are not blocked. Don't treat `GpgUnavailable` as a failure — log a debug message at most |

---

## Testing Checklist (Manual)

After any change, build and verify:

1. `cmake --build --preset dev` → zero errors. `ctest --test-dir build/dev --output-on-failure` → all green.
2. `sudo cmake --install build/dev`.
3. Right-click an uninstalled `.AppImage` in Dolphin → Open With → AppImage Manager → window opens at its fixed size (no "Manage AppImage" entry any more).
4. Drag icon to folder → install animates → folder column fades → icon centers.
5. Click installed icon → app launches.
6. Click Remove → `UninstallDialog` opens inline → corpse list populates → trash works → window closes.
7. `appimagemanager` (no args) → DashboardWindow opens, scans, shows list with icons. Drop a new `.AppImage` into the apps dir → row inserts live (no full reload).
8. Dashboard search → filters live. Sort button (next to search) → by name/size/category/date → list re-orders.
9. Dashboard detail `⋯` → Remove… → `UninstallDialog` → list refreshes after trash.
10. ⟳ in the toolbar (or main menu → Check for Updates) → thin progress bar and a toast "Checking for updates (x/y)" counting up; none found → the toast says "All AppImages are up to date" and fades; network off → error message above the list with Retry. Update found → the row moves under "Updates available" with "old → new" in green, "Installed (N)" tab, "Update to X" button → download starts directly, progress replaces the button; with 2+ updates "Update All" in the toolbar starts them all. An app with zsync updates downloads only the changed part (debug log: "zsync: N of M blocks local").
11. Settings → browse button opens folder picker → valid path saved → invalid path shows error.
12. Settings → disclaimer toggle persists across restart.
13. Select an AppImage with AppStream metadata (e.g. ProtonUp-Qt) → one dim metadata line, description flows inline below the actions; one scrollbar for the whole detail pane; no horizontal scrollbar.
14. Settings → "Notify when an AppImage is downloaded" on → copy a `.AppImage` into `~/Downloads` → KDE notification fires after settle pass → click "Manage" → ManageWindow opens for that file.
15. Toggle download notification off → copy another AppImage to `~/Downloads` → no notification fires.
16. Start daemon (`appimagemanager --daemon`) → tray entry appears with "Check Now" + "Open Dashboard". Open dashboard while daemon runs → drop a file in `~/Downloads` → only one notification fires (daemon wins).
17. AboutDialog: open dialog → disable network (`nmcli networking off`) → reopen About → wait 8 s → spinner hides, fallback contributors shown, no SIGABRT. Re-enable network, reopen → real contributors load.
18. Sandbox: run `FLATPAK_ID=test appimagemanager`, open Settings → "Download watching is unavailable in sandboxed environments." InlineMessage visible below toggle. Copy a `.AppImage` to `~/Downloads` → no notification fires.
19. Update: trigger update on a test AppImage → verify `.bak` file created mid-swap; on success `.bak` removed. Kill process during swap → `.bak` present afterward for manual recovery.
20. Launch app → Installed tab shown immediately; Discover NOT yet loaded (no network traffic at startup).
21. Click Discover → spinner visible immediately → list populates after a few seconds with no UI freeze. Close and reopen Discover → loads instantly from cache.
22. Discover search → type "firefox" → results filter live. Sort button → Sort A–Z / Z–A; Source → "AppImage Hub" → only Hub apps; Category → pick one. Active filters show as a dim line under the search; ✕ clears them. Changing a filter clears the selection.
23. Discover: app with both AM + Hub → "Install" plus `⋯` offering "Download from AppImage Hub". App with only AM → "Install", `⋯` with View Install Script. Details: About, then Category, License, Available from, Links; the list and its width match Installed. App with only a Hub download → "Download".
24. Settings: sidebar with General, Updates, Security, Discover; each page a centered form; Restore Defaults under the sidebar. Header: Settings and About open from the main menu with Back + title in place of the tabs; Back and Alt+Left return to the last tab; Ctrl+, opens Settings. Settings → Restore Defaults (under the sidebar) resets values and the Enable Discover switch, but does not bring back the welcome dialog.
25. Discover: click Install → progress bar + Cancel replace the button, one dim status line; "Show Details" shows the plain log. A failed install opens the log by itself. Cancel mid-install → "Installation cancelled." in the log, button resets.
26. Settings → enable "Verify GPG signatures". Drag an unsigned AppImage to install → `signatureWarnDialog` appears with "No Signature Found" title and security-medium icon. Click Cancel → no install. Click OK → install proceeds normally.
27. With signature verification on, drag an AppImage that has a valid GPG signature → no dialog, installs directly.
28. With signature verification OFF → drag any AppImage → installs immediately, no dialog regardless of signature state.
29. Install two AppImages with the same display name but different filenames, neither having a reverse-domain `appId` → both get unique `.desktop` files in `~/.local/share/applications/` (slugs differ by hash suffix); `isDesktopLinkEnabled` correctly tracks each independently.
30. Update source: pick an AppImage with no built-in update info → detail `⋯` → Update Source… → paste a GitHub `owner/repo`, a Codeberg/Forgejo repository URL, a `.zsync` URL or a direct `.AppImage` URL → the line under the field names the detected kind (Type menu overrides) → Save → Details shows it under Updates → "Check for Updates" → a newer release (or a newer build for a direct URL) is offered; Update downloads it. A second check reports it up to date. "foo" shows the error. "Use Built-in" clears it.
31. Update source with "Include pre-releases" on a repo whose newest release is a pre-release → that version is offered; off → only the latest stable.
32. Fresh config (`mv ~/.config/appimagemanagerrc{,.bak}`) → single-page onboarding with Discover off → Discover page shows "Discover is turned off" and no network requests happen; "Turn On Discover" loads the catalogs. Get Started without enabling → Discover stays off. Restore the config afterwards.
33. `cd packaging && makepkg -si` → package builds and installs; `pacman -Ql appimage-manager-plasma` lists the binary and the QML module.
34. Environment: detail `⋯` → Environment Variables… → `WEBKIT_DISABLE_DMABUF_RENDERER=1` → Save → Details lists it; Launch and the app menu entry both start the app with it (`cat /proc/<pid>/environ`). A line like `not a var` is refused with its line number.
35. Direct download URL that answers with a web page (e.g. a project's download landing page) → the check fails quietly / the download is refused and the installed AppImage stays intact.
36. Sort Installed by category and by date added → section headers appear; Discover without a category filter shows category headers, with one filtered none.
37. Settings → Catalogs: drag a catalog by its handle → it moves and the new order sticks after reopening Settings. With Discover filtered to a catalog, reorder or remove another one → the filter stays on the same catalog. Edit a turned-off catalog → it stays off.
38. Drag an AppImage over the window on Discover, Settings, and an empty Installed list → the whole window highlights with "Drop to install"; on Installed with nothing selected (overview with its drop box) only the box highlights; with an app selected the whole window does.
39. An app that writes its own menu entry (e.g. ArtCraft's `ai.storyteller.*`) next to an old `appimage_*` entry → open the dashboard → only the app's entry remains, "Show in App Menu" is on; turning it off removes it (Trash), on writes `<appId>.desktop` again.
40. Tray (Plasma): after installing, restart plasmashell (`systemctl --user restart plasma-plasmashell`) → the AppImage Manager widget sits in the system tray (extra items while nothing waits) and the daemon shows no second icon. With an auto-hiding panel, opening it keeps the panel shown. Without the widget (other desktops, or removed from the tray): `appimagemanager --daemon` → our KStatusNotifierItem after ~8 s. Left click → popup / window next to the tray listing all AppImages with their state and a one-line header "No updates available · Checked …" (next check only in the tooltip); Esc or clicking elsewhere closes it; a row opens the main window with that app selected; the gear opens Settings. Middle click checks. Right click: Open AppImage Manager, Check for Updates, Settings…, Quit. With updates: count badge, one notification listing them (not repeated at the next check), Update All in it and in the window updates them with progress and one "Updated N" notification. Restart the daemon → the window still shows the last results and the same next-check time. Update an app in the main window → the tray's count drops.
41. Status: after a check, every Installed row says Up to date / No update source / Check failed (warning icon, reason on hover) / old → new version in green; updates sit on top under "Updates available"; Sort → By Status groups all states. With nothing selected the overview shows counts (click one → list filtered, dim filter line with ✕), total size, "Last checked …", Check Now, Update All; Settings → General → Overview off removes it. Details: Updates line says what the check found, "Set Update Source…" without a source, App menu Shown/Not shown, Category in plain words. Check in the main window → the tray shows the same results without checking itself (and the reverse).
42. Install window with the app already installed (another file name, older version) → "Version X is installed · this one is newer" in green; drop → "Replace Older Version?" with Replace / Keep Both / Cancel. Replace → the old file is in the Trash, the new one keeps its update source (Details) and environment, and the app menu entry only if the old one had it. An older file → "this one is older", "Install Older Version?". The same file again (from another folder) → "This version is already installed", no Keep Both.
43. Install window: Ctrl+Shift+→ → the icon slides into the folder, the folder grows as on a drag, then it installs (or the replace dialog opens). Hint reads "Drag icon to install (Ctrl+Shift+→)". Installed → Enter starts the app; with a dialog open, neither shortcut fires.
44. Settings folder: ⋯ → Keep Settings in “Applications” on an app with settings in ~/.config → the dialog lists them → Turn On → they move to `Applications/.config/<app>/`, Details → Settings shows the folder, the app starts with its settings (launch from here and from the app menu). Turn off → offered to move back. Settings → General → App settings on → a newly installed app gets its own folder. Remove → the folder is listed in the leftovers.
45. Undo update: update an app → `Applications/.previous/<file>` holds the old one, ⋯ shows "Restore Previous Version (old version)" → the old version is back (Details), the row is Not checked; again → the new one is back. Update again → the kept copy is the one just replaced. Remove → the kept copy is listed in the leftovers.
46. Sandbox: ⋯ → Sandbox… → on, add ~/Music → Save → Details: Sandbox On → Launch: the app starts (first launch mounts it), its open dialog shows an empty home except Music, it keeps its settings across starts (in Applications/.config/<app>), `ps` shows `bwrap`. The app menu entry's Exec is `appimagemanager --run <file>` and "Show in App Menu" stays on. Without bubblewrap: the dialog warns, the switch is off.
47. Signature: select an app → Details → Signature says "Checking…" then "Not signed" (most AppImages) or "Signed · key XXXX XXXX XXXX XXXX" (tooltip: full fingerprint); a signed file changed afterwards → red "Does not match its signature". With "Verify GPG signatures" on, installing an unsigned AppImage warns "No Signature Found" (not "Invalid").
48. Settings → Security → Sandbox all AppImages on → every app's Details says Sandbox On and its menu entry starts `appimagemanager --run`; one app's Sandbox… off → only that one runs normally. Shared folders: tick Downloads → a sandboxed app sees ~/Downloads. Without bubblewrap the switch is disabled.
49. Installed list: drag the separator → the list resizes (not past 11 gridUnits or into the details' 20); restart → same width; double-click → default.
50. Release notes: app with a GitHub source → ⋯ → Release Notes… → spinner, then the releases newer than the installed version with headings, dates and formatted notes; links open in the browser; Open on GitHub opens the newest release. Network off → "No release notes" with the reason. zsync/direct sources: no menu item. Details → Updates: the GitHub/Codeberg label is a link to the repository.
51. App menu: after installing, open the dashboard once → existing entries are rewritten (`X-AppImageManager-Entry=2` in `~/.local/share/applications/*.desktop`); an app that opens files (e.g. an image viewer) appears in Dolphin's Open With for its types; right-click it in Kickoff → its own actions (if any) and "Manage in AppImage Manager", which opens its page. The description in the menu is the app's own (translated).
52. ⋯ → Don't Update → row says "Pinned", overview has "1 pinned (never updated)", Check for Updates skips it (the toast counts one fewer), the tray lists it under "Not tracked". Un-pin → "Not checked yet", the next check includes it.
53. Sandbox… → turn "Allow network access" off → Details "On · no network" → the app can't reach the internet (a browser shows no connection), display and sound work. On again → normal.
54. Update check of GitHub apps with no newer release uses no API requests (`curl -s https://api.github.com/rate_limit` before and after shows the same remaining count).
