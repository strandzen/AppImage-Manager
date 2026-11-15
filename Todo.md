# To-do tasks

Open tasks are grouped by priority (High / Medium / Low) rather than feature area. When adding a new one, say what priority it should be (or it'll be asked). Finished items move to **Completed** (grouped by feature area); abandoned ones move to **Abandoned**.

## High priority

(Nothing open.)

## Medium priority

(Nothing open.)

## Low priority

(Nothing open.)

## Priority not set

Moved from CLAUDE.md's old Roadmap section on 2026-11-14. Waiting for a priority.

- **Per-item signature badge in LibraryPage** - lazy-check signature state for each installed AppImage and show a badge in the detail pane (verification on install is done; the per-library scan is the remaining piece).
- **Sandbox/bubblewrap launch option** - opt-in setting to launch AppImages under `bwrap` for filesystem isolation.
- **Dolphin plugin vs servicemenu** - both ship and both add "Manage AppImage" (same command). Decide whether to keep one (the servicemenu needs no compiled code that must match Dolphin's KIO version).

## Abandoned

(Nothing yet.)

## Completed

### Audit after the revamp (2026-11-15)
- Removed dead code left from before the revamp and fixed the bugs found (catalog reorder, Discover source filter, update-check time-out, architecture matching, drop highlight, uninstall dialog). See `markdown/plans/audit_cleanup_post_revamp.md`.

### From Shelly, round 2 (2026-11-14)
- Update sources beyond GitHub: Codeberg/Forgejo/Gitea, zsync URL, direct download URL (auto-detected input). GitLab left out for now.
- Per-AppImage launch environment variables.
- Content-Type check so a web page never replaces an AppImage.
- Visual pass: About/Details sections, list pane as a panel, section headers in lists.

### UI overhaul (2026-11-14)
- Cleaner, quieter UI in stock Kirigami style: header tabs instead of the sidebar, one-line metadata, primary action plus an overflow menu, non-modal update feedback, Update All, single Discover filter menu, quiet install progress with the log on demand, regrouped Settings, single-page onboarding, restyled manage window. Removed the accent-borders and icon-size settings, `UpdateDialog.qml` and `Theme.qml`.

### Revamp (2026-11-14)
- Per-AppImage update source (from Shelly): set a GitHub repo for any installed AppImage, optional pre-releases, arch-aware release asset pick, direct `.AppImage` downloads when a release has no `.zsync`.
- Onboarding: new "Discover Apps" step. Discover is opt-in for new installs (no catalog downloads until enabled); choose which catalogs it searches.
- `cd packaging && makepkg -si` replaces `./install.sh`.
- Clean up: removed unused `SettingsDialog.qml`, About content shared between page and dialog, stale `packaging/aur/appimagemanager/` removed and PKGBUILD deps synced (added missing `qt6-svg`), `GEMINI.md` removed.
