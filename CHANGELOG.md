# Changelog

## Unreleased

## 2.2.0 - 2026-11-17

### Added
- On Plasma, a system tray widget: it opens as a normal panel popup (the panel stays visible, Plasma's look), shows the update count on its icon, and is hidden among the tray's extra items when nothing needs a look. On other desktops a tray icon with its own window does the same.
- Tray overview: click the tray entry for an overview of your AppImages and their update state (update available, up to date, no update source, check failed), when they were last checked (the tooltip also says when the next check is due), with Check Now, Update All, and buttons for the main window and Settings. Clicking an app opens the main window with it selected. Middle-click checks for updates.
- The tray icon shows the number of waiting updates, and its tooltip says when it last checked and will check next.
- A Check for Updates button (⟳) in the main window's toolbar.
- While updates are checked, a note at the bottom of the main window counts up ("Checking for updates (3/13)") and then shows the result, so a long check no longer looks stuck.
- Details show each app's signature: signed (with the key's fingerprint), not signed, or changed since it was signed. It's checked in the background when you select the app.
- Settings → Security → Sandbox all AppImages runs every app in the sandbox (leave one out in its Sandbox… dialog), with shared folders every sandboxed app may use: Downloads, Documents, Pictures, Music, Videos, Desktop, or your own.
- Details: the update source of a GitHub, Codeberg, Forgejo or Gitea app links to its repository.
- Release notes: ⋯ → Release Notes… shows what changed in every release newer than the installed one (or the latest release), formatted, with a link to the release page. Fetched when you open it, for GitHub, Codeberg, Forgejo and Gitea sources.
- The Installed list can be made wider or narrower by dragging its edge; double-click the edge for the default width. The width is remembered.
- Sandbox: ⋯ → Sandbox… runs an app with bubblewrap. It sees only its own folder in `Applications/.config` and the folders you add for it; the rest of your home folder and other drives are hidden, display, sound and network keep working. Launching from AppImage Manager and from the app menu both use it.
- zsync updates no longer need `zsync2`: AppImage Manager reuses the parts of the installed file that didn't change and downloads only the rest (for example 320 KB instead of 36 MB), and checks the result. If a server can't send parts of a file, it downloads the whole file instead.
- Undo an update: the version an update replaced is kept (in `Applications/.previous`) until the next update, and ⋯ → Restore Previous Version brings it back. Restoring again swaps them back.
- Keep an app's settings in the Applications folder (`Applications/.config/<app>`) instead of `~/.config`, so one folder holds your apps and their settings. Turn it on per app with ⋯ → Keep Settings in Applications (it offers to move the current settings along), or for every new install in Settings → General. The folder is offered with the other leftovers when you remove the app.
- Install from the keyboard: in the install window, Ctrl+Shift+→ slides the app into Applications, just like dragging it. Once it's installed, Enter starts it.
- `appimagemanager --settings` and `--select <path>` open the main window on Settings or with an AppImage selected.
- The install window says when the app is already installed, and whether this file is newer, older or the same version. Dropping it on Applications then asks: Replace (the installed copy goes to the Trash, its update source, environment variables and app menu entry move to the new one), Keep Both, or Cancel.

### Added (overview)
- Every app in the Installed list says where it stands: "Up to date", "No update source", "Check failed" (with a warning icon; the reason on hover), or the new version in green ("1.17.0 → 5.14.0") instead of a dot.
- Apps with an update are listed first, under "Updates available", whatever the sort. The new "By Status" sort groups the whole list by update state.
- An overview next to the list while no app is selected: how many AppImages, their total size, how many have updates, are up to date, failed or have no update source (click one to show only those), and when updates were last checked, with Check Now and Update All. It can be turned off in Settings → General.
- Details say what the last check found ("Up to date · checked 14:02", "Check failed: Repository not found"), offer "Set Update Source…" when there is none, and show whether the app is in the app menu.
- The ⟳ button's tooltip says when updates were last checked.
- Two AppImages of the same app: the older one is marked "Older copy of …".

### Changed
- Translations: every missing string in the 15 languages now has a machine-made draft, marked fuzzy for native speakers to review (fuzzy strings still show in English).
- Dolphin: AppImages open with AppImage Manager through **Open With**; the separate "Manage AppImage" right-click entry (a Dolphin plugin and a service menu that both added it) is gone.
- The main window and the tray share check results: what one checked, the other shows, including the reason a check failed.
- Categories are shown in plain words ("Graphics", "Internet") instead of the raw list ("Graphics, 2DGraphics, VectorGraphics, X-Design").
- Updates in progress show a progress bar in the list row.
- The tray lists apps with updates first and folds the ones without an update source into "Not tracked (n)".
- Update checks run in parallel (six at a time) instead of one after another, so checking many AppImages takes a fraction of the time.
- Settings are laid out like a KDE settings page: General, Updates, Security and Discover in a list on the left, each a form with labels on the left and short explanations under the options. Restore Defaults is under the list.
- One notification for new updates instead of one per AppImage, with Update All and Open buttons; a version is announced only once, not again on every check. Update All from the tray ends with one summary notification.
- The tray's right-click menu: Open AppImage Manager, Check for Updates, Settings.
- The main window shows the updates the background checker found, and the tray notices updates and installs done in the main window.
- The background checker also checks AppImages that update through a plain zsync link.

### Fixed
- zsync updates stay quick when much of an app changed: blocks are matched in pairs like zsync does, and when little can be reused the whole file is downloaded instead of many small pieces.
- Signature checks: every unsigned AppImage was reported as having an invalid signature (the empty signature space every AppImage has was taken for a broken signature), and signed ones were checked against the wrong data. Signatures are now checked the way appimagetool makes them, against the key in the file.
- The install window keeps its small fixed size. Window managers and scripts that restore window sizes (such as KWin's "Remember Window Positions") could widen it to the main window's size; it now has its own title ("<App> – AppImage Manager") and fixed size limits.
- Settings changed in the main window (check frequency, Applications folder, download notifications) now reach the background checker within a minute; before, it kept the old ones until it was restarted.
- Restore Defaults no longer brings back the welcome dialog (which also turned Discover off).
- App menu entries for AppImages whose path or environment contains a `%` work.
- Closing the main window crashed AppImage Manager (`free(): invalid size`).
- The background checker could not be started on demand on systems using dbus-broker (Arch and others): its D-Bus service file used a section name dbus-broker does not accept. The tray widget now also says when the checker is not running instead of showing an empty list.
- Background checks now keep to their schedule across restarts and suspend: "Weekly" checked at every login, and "Monthly" and custom intervals over 24 days did not work at all. Checks wait while offline and run once you are back online.
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
- Update checks on a large library reported a time-out: the 30-second limit covered all AppImages together. After a real time-out, late results no longer leak into the next check.
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
