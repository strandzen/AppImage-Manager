# GUI Improvement Plan — AppImage Manager

**Focus:** Smoother, faster, more polished feel + feed.json integration.

---

## Context

The app has solid bones: master-detail layout, sidebar navigation, store page, library page, settings, about. The AM/AM-GUI integration works — you can browse and install from the AM catalog. The issues are mostly **feel**: transitions are missing or abrupt, the Store has blank icons and rough edges, and there is a better catalog source available. This plan addresses all of it.

---

## What Already Works Well (don't touch)

- Sidebar collapse animation (smooth, 300ms)
- Library list add/remove/displace animations
- Drag-to-install interaction in ManageWindow
- Keyboard shortcuts throughout
- Kirigami theming is consistent

---

## New Data Source: appimage.github.io/feed.json

This is the AppImage Hub catalog — 1,383 curated apps with:
- **Real icons** hosted at `https://appimage.github.io/<AppName>/icons/<size>/<name>.png`
- **Proper categories** (AppStream standard: Audio, Development, Game, Graphics, Network, Office, System, Utility, etc. — 40+ categories total)
- **Descriptions** (1,218 of 1,383 apps have one)
- **Authors + license**
- **Links**: GitHub repo, Download URL, and an Install link (AppImage-specific)

**Compared to the current AM plain-text database:**
| Source | feed.json | AM database |
| --- | --- | --- |
| App count | 1,383 (curated) | ~3,000+ |
| Icons | Real URLs | None (system theme fallback) |
| Categories | Proper AppStream | Heuristic guessing |
| Descriptions | 88% coverage | Short one-liners |
| Install method | Download URL | appman CLI script |

**Plan: use both, but let the user choose.** At the top of the Store page, a segmented source picker:

- **AppImage Hub** — feed.json only (1,383 curated apps, real icons, proper categories)
- **AM Database** — AM catalog only (3,000+ apps, install scripts, less metadata)
- **All** — merged view; for apps in both sources, the detail pane shows which sources are available and lets the user pick which one's metadata to display

The selected source persists across sessions (saved to settings). Default: All.

---

## Improvements

---

### 1. Feed.json Integration (HIGH IMPACT)

**Problem:** The Store currently shows a wall of apps with no icons, guessed categories, and thin descriptions. It looks like a raw text dump.

**What changes:**
- On Store open, fetch `https://appimage.github.io/feed.json` alongside the AM database
- Cross-reference apps by name — feed.json's curated metadata replaces the heuristic-guessed data
- Categories in the Store become real AppStream categories (Audio, Development, Game, etc.) instead of the hardcoded 7-item list
- Descriptions become proper multi-sentence text where available
- Install method: if the app has an AM script → use `appman -i`. If not → use the Download link from feed.json directly

**Result:** The Store feels like a real app catalog, not a debug list.

**Where:** `AMStoreModel.cpp/h` — add a second fetch + merge logic. Store renamed internally to reflect dual source.

---

### 2. Store App Icons (HIGH IMPACT)

**Problem:** Nearly every app in the Store shows a blank placeholder icon. The current code tries to find an icon by package name in the system icon theme, which almost never matches.

**Fix:** Use icons from the feed.json. Each app has a hosted icon URL. Load these lazily (only for visible rows) and cache to disk so they don't re-download.

**Icon priority order:**
1. System theme icon (if found by name — fastest, already correct size)
2. feed.json icon URL (fetched async, cached locally)
3. Generic AppImage placeholder (last resort)

When an icon loads from the network, it fades in (150ms opacity transition) — no jarring pop-in.

**Where:** `AMStoreModel.cpp` — add per-row async icon fetch + disk cache. `StorePage.qml` — use the icon source with a fade-in `Behavior`.

---

### 3. Detail Pane Transitions (HIGH IMPACT, easy)

**Problem:** Clicking an app in the Library or Store snaps the detail pane to new content instantly. It's jarring and makes the app feel unresponsive.

**Fix:** Short opacity fade (150–200ms) when detail content changes. The pane stays in place — only the content fades out and fades back in. Single biggest feel improvement relative to effort.

**Where:** `LibraryPage.qml` and `StorePage.qml` detail columns.

---

### 4. Installation Progress Bar (HIGH IMPACT)

**Problem:** Installing from the Store shows a raw scrolling console log with no sense of progress. You can't tell if it's 10% or 90% done.

**Fix:** Parse `appman` output for stage markers (Fetching → Downloading → Extracting → Installing → Done) and map to a `ProgressBar` shown above the console. The console log stays visible — the progress bar sits above it. There's a small "show/hide log" toggle button so users who want the raw output can always see it, and users who don't aren't overwhelmed.

**Where:** `AMStoreModel.cpp` — detect stage keywords in log lines, emit a `stageProgress(int 0–5)` signal. `StorePage.qml` — `ProgressBar` + collapsible log section.

---

### 5. Color-Coded Install Console (MEDIUM IMPACT, easy)

**Problem:** Install log is plain white monospace text. Errors look identical to progress messages.

**Fix:** Colorize per line as output arrives:
- Lines containing `error` / `failed` / `✗` → red
- Lines containing `warning` → orange  
- Lines containing `success` / `done` / `✓` / `installed` → green
- Everything else → default text color

**Where:** `StorePage.qml` — check each line in the log `Repeater` for keywords, set `color` accordingly.

---

### 6. Cancel Button for Store Installs (MEDIUM IMPACT, easy)

**Problem:** Once you click Install there is no escape. The button says "Installing…" and you wait.

**Fix:** While installation is running, the Install button becomes a Cancel button. Clicking it kills the `appman` process and resets state cleanly. Log is cleared.

**Where:** `AMStoreModel.cpp` — expose `cancelInstallation()` slot. `StorePage.qml` — swap button label/action based on `isInstalling` state.

---

### 7. Store Database Caching (MEDIUM IMPACT, easy)

**Problem:** Every Store open re-fetches the entire AM database and feed.json from the internet. Noticeable delay, wasted bandwidth.

**Fix:** Cache both files locally (`~/.local/share/AppImageManager/`). On open, load from cache immediately (instant), then silently check in the background if newer versions exist. Only re-fetch if cache is older than 24 hours, or user explicitly clicks Sync.

**Where:** `AMStoreModel.cpp` — file-based cache check before any network request.

---

### 8. Category Filter Upgrade (MEDIUM IMPACT)

**Problem:** The Store's category chips are a hardcoded list of 7 items. Many apps don't fit, many real categories are missing.

**Fix:** Build the category list dynamically from the actual app data (which now comes from feed.json's proper AppStream categories). Show only categories that have at least one app. Group minor categories under a collapsible "More" chip.

**Where:** `AMStoreModel.cpp` — expose available categories as a property. `StorePage.qml` — replace the hardcoded `ListModel` with the dynamic list.

---

### 9. Category Chip Visual Feedback (LOW-MEDIUM IMPACT, easy)

**Problem:** Tapping a category chip has no animation. The filter applies but the chip selection looks like it snapped.

**Fix:** Add a brief scale pulse on tap (chip scales to 0.95 → 1.0, 100ms). Already-selected state is fine — just the tap response is missing.

**Where:** `StorePage.qml` — add `scale` Behavior on the category chip buttons.

---

### 10. Stale Log Cleanup After Install (LOW IMPACT, easy)

**Problem:** After install finishes, the console log stays with old output. Navigate away and back — stale output still there.

**Fix:** Auto-clear log lines when a different app is selected. Add a small "Clear" button after install completes for manual clearing.

**Where:** `StorePage.qml` — reset `logLines` when `currentIndex` changes.

---

### 11. Settings: Animate Conditional Fields (LOW IMPACT, easy)

**Problem:** The custom day-count spinbox (for "Custom" update frequency) appears and disappears instantly.

**Fix:** Opacity + height animation (100ms). Fades in/out instead of snapping.

**Where:** `SettingsPage.qml` — `Behavior on opacity` + `Behavior on implicitHeight` on the conditional row.

---

### 12. Empty State Polish (LOW IMPACT, easy)

**Problem:** Store "no results" state is a plain text label. Doesn't match the quality of the Library empty state.

**Fix:** Center-aligned icon + heading + subtext, matching Library empty state style.

**Where:** `StorePage.qml`.

---

## Suggested Order of Work

| # | Item | Why this order |
|---|------|----------------|
| 1 | Feed.json integration | Foundation — icons and categories depend on it |
| 2 | Store app icons | Visible immediately, transforms the Store page |
| 3 | Detail pane transitions | Biggest feel improvement per minute of work |
| 4 | Color-coded console | Easy win on top of existing console |
| 5 | Cancel install button | Removes a UX trap |
| 6 | DB caching | Removes startup delay |
| 7 | Progress bar | Polish once install flow is solid |
| 8 | Dynamic categories | Depends on feed.json data being in place |
| 9–12 | Rest | Minor polish, do last |

---

## Verification

After each change: `cmake --build --preset dev && sudo cmake --install build/dev`

- Open Store → all/most apps show real icons (not placeholders)
- Select an app → detail content fades in smoothly
- Install an app → colored log + progress bar; cancel mid-install works
- Close and reopen Store → loads instantly from cache
- Store search with no results → clean empty state
- Settings → switch to Custom frequency → spinbox fades in
