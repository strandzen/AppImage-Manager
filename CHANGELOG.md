# Changelog

## Unreleased

### Fixed
- Apps showed up twice in the app menu when they (or a launcher such as ArtCraft Launcher) add their own menu entry: AppImage Manager added a second one beside it. It now recognises any entry that starts the AppImage, names its own entry after the app's id (for example `ai.storyteller.figcraft.desktop`) so an app that registers itself replaces it instead of adding another, and removes its own duplicates on the next start. Keeping the app's entry also keeps its file associations and taskbar icon matching. Turning off "Show in App Menu" removes every entry for the AppImage (ones written by other software go to the Trash).

## 2.1.0 - 2026-11-15

### Added
- Update Source accepts Codeberg, Forgejo and Gitea repositories, a `.zsync` file URL, or a direct download URL, besides GitHub. Paste a repository or URL and the kind is detected (with a Type menu to override it). Direct downloads count as updated when the server reports a newer build. The background checker handles these too.
- Environment variables per AppImage (for example `WEBKIT_DISABLE_DMABUF_RENDERER=1` for apps that show a blank window), used when the app is launched from AppImage Manager or from the app menu.
- Installed details are split into About and a Details list (version, size, categories, date added, update source, environment, file).
- Section headers in lists: Installed groups by category or date added when sorted that way; Discover groups by category.
- Update Source for each installed AppImage: point it at the GitHub repository it is released from, optionally including pre-releases. Works for AppImages without built-in update information.
- GitHub updates pick the release file for your CPU architecture and can download a plain `.AppImage` when the release has no `.zsync` file.
- Onboarding step to turn on Discover and choose its catalogs.
- "Enable Discover" switch in Settings.

### Changed
- Cleaner, quieter dashboard: the sidebar is replaced by Installed and Discover tabs in the toolbar, with Settings and About in the main menu. The Installed tab shows how many updates are waiting.
- Installed page: rows show only icon, name and version; the details show one line of metadata, Launch, Update to the new version, and a menu for Show in App Menu, Update Source and Remove.
- Updates start straight from the Update button (no confirmation dialog), and Update All appears when several are available.
- Discover: one Sort and Filter menu (sort, source, category) replaces three buttons; active filters show under the search field with a clear button. Installing shows a progress bar, one status line and Cancel; the full log is behind "Show Details" and opens by itself when an install fails. Apps without an icon get a generic icon.
- Settings are regrouped into General, Updates, Security, Discover and Catalogs (catalogs only show while Discover is on), with shorter descriptions. Restore Defaults is a button at the bottom.
- Onboarding is a single page: Applications folder, Downloads notifications, and Discover with its catalogs.
- The Installed drop zone is an outlined box again: it fills the space below the list with its label centred, and stays visible at the bottom when the list is long.
- The list pane in Installed and Discover has its own background, so list and details read as two areas. Discover rows show the author instead of the category.
- The manage window drops the bordered box and chips: one line shows version and size, and the icons use one fixed size.
- Update check results no longer open dialogs: "up to date" is a short notification, errors show above the list with Retry.
- Discover is off by default for new installs, so no app catalogs are downloaded until you turn it on. Existing installs keep it on.
- Local installs on Arch use `cd packaging && makepkg -si` (a removable pacman package) instead of `./install.sh`.
- The About dialog in the manage window shows the same content as the dashboard's About page.

### Fixed
- Installed apps showed an unrelated theme icon (for example the "ai" icon) instead of their own: app ids from reverse-DNS names like `ai.storyteller.figcraft` were cut at the first dot. Everything is re-read once after updating.
- List selection highlights ran under the scrollbar in Discover, Installed and the uninstall dialog; the leftover-files list in the uninstall dialog had no scrollbar.
- Mouse-wheel scrolling in long lists (Discover) is faster and smoother; Discover icons are decoded in the background at the size shown.
- Updates that download an `.AppImage` directly refuse a web page (for example a login or error page) instead of replacing the installed app with it.
- Settings: Restore Defaults was unreachable, and did not refresh the Enable Discover switch.
- Discover kept showing the previously selected app after the list was filtered.
- Sorting the Installed list was unreachable; it is now a button next to the search field.
- AUR package was missing the `qt6-svg` dependency.
- Settings → Catalogs: dragging a catalog to reorder it did nothing, and editing a turned-off catalog turned it back on.
- Discover's Source filter switched to another catalog after catalogs were reordered or removed.
- Update checks on a large library reported a time-out: the 30-second limit covered all AppImages together instead of each one. After a real time-out, late results no longer leak into the next check.
- Release files whose name contains "arm" (like `Swarm.AppImage`) were skipped on x86_64, so no update was found for them.
- Dragging an AppImage over the window showed no highlight on an empty Installed list, on Discover or on Settings. The "Drop to install" text was hard to read on light themes.
- Uninstall dialog: the All and None buttons could stay hidden, leftover folders over 2 GiB showed a wrong size, and paths were not shortened to `~`.
- The dashboard no longer contacts GitHub at startup to load the About page's contributor list; it does so when About is opened.

### Removed
- The update confirmation dialog.
- The "accent borders" and "manage window icon size" settings.
- `install.sh`, the unused settings dialog, and the stale `appimagemanager` PKGBUILD.

## 2.0.1

First entry in this file. See the GitHub releases for earlier history.
