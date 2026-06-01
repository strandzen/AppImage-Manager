# Feasibility: Integrate AppImageManager Dashboard into Shelly-ALPM

## What Shelly-ALPM is

C# / .NET 10 + GTK4 Arch Linux package manager. Manages native packages, AUR, Flatpak, and AppImages. Its AppImage handler is fully self-contained:
- Installs to `/opt/shelly/` (requires sudo/elevated)
- Stores metadata in `~/.cache/shelly/appimage-metadata.db` (JSON)
- Extracts via `--appimage-extract` subprocess
- Multi-source update checks: GitHub, GitLab, Codeberg, Forgejo, static URL
- Own GTK4 UI tab (list, detail, configure-updates, upgrade-all)
- Own CLI: `shelly appimage install/remove/upgrade/list`

## What AppImageManager is

C++ / Qt6 / Kirigami / KDE Plasma 6. Manages AppImages only:
- Installs to `~/Applications` (user-space, no sudo, KIO::CopyJob)
- No metadata DB — filesystem scanning only
- Extracts via libappimage / squashfuse
- UpdateDaemon checks GitHub Releases API only
- KDE-native UI (Kirigami, Dolphin plugin)
- D-Bus interface: `io.github.appimagemanager.Manager1` — read-only (`InstalledAppImages`, `Version`)

---

## Verdict: Not Feasible as a Direct Replacement

### Fundamental mismatches

| Concern | Shelly | AppImageManager |
|---------|--------|-----------------|
| UI toolkit | GTK4 | Qt6 / Kirigami |
| Target DE | GNOME / generic | KDE Plasma 6 |
| Language | C# / .NET 10 | C++ |
| Install location | `/opt/shelly/` (root) | `~/Applications` (user) |
| Metadata | JSON DB with update URLs | Filesystem scan only |
| Update sources | GitHub + GitLab + Codeberg + Forgejo + static | GitHub only (UpdateDaemon) |
| Privilege model | sudo elevation | KIO (no sudo) |
| D-Bus | None | Read-only query only |

### Why replacement fails

1. **Language boundary** — C# cannot embed Qt widgets. Only IPC is possible (D-Bus or subprocess). AppImageManager's D-Bus interface is read-only; it cannot be driven as a backend.

2. **Install location conflict** — Shelly puts AppImages in `/opt/shelly/` (system-wide, requires root). AppImageManager targets `~/Applications` (user-only). Scanning each other's directories would require config changes on both sides.

3. **Metadata gap** — Shelly tracks update URLs per-AppImage in its DB. AppImageManager has no equivalent persistence. Replacing Shelly's handler would silently drop all update configurations.

4. **Update source coverage** — AppImageManager's UpdateDaemon only checks GitHub. Shelly supports 5 sources. Replacing Shelly's handler degrades update functionality.

5. **Privilege model** — Shelly's install is elevated (writes to `/opt`, `/usr/share`). AppImageManager deliberately avoids root. These are incompatible philosophies.

### What IS feasible (shallow integration only)

If the goal is simply "launch AppImageManager from Shelly for KDE users":

- Shelly could detect KDE session (`XDG_CURRENT_DESKTOP=KDE`) and `exec appimagemanager <path>` instead of its own install flow — a 10-line conditional in `PrivilegedOperationService.cs`
- Or: register AppImageManager as the XDG MIME handler for `application/x-iso9660-appimage` so the OS routes `.AppImage` opens to AppImageManager instead of Shelly

Neither approach replaces Shelly's handler — they bypass it for KDE users only, while Shelly retains its own logic for GNOME/GTK users.

## Recommendation

**Don't integrate.** The two tools serve different ecosystems (GTK/GNOME vs KDE), have incompatible install conventions, and non-overlapping metadata models. Integration cost is high, benefit is low, and it would make AppImageManager dependent on a C#/.NET project that targets a different desktop.

If any cross-project value exists, it would be AppImageManager exposing a richer D-Bus interface (install, remove, update) so Shelly or other tools could optionally delegate to it on KDE — but that's additive work on AppImageManager's side, not an integration with Shelly.
