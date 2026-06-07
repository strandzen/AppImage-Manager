# Plan: Discover Page — Freeze, Alignment, Icon Fallback

## Context

Three user-reported issues in the Discover (AMStore) page that block polish:

1. **UI freeze** on first Discover open — main thread blocked by `mergeDatabases()`
2. **Loading indicator misalignment** — BusyIndicator sits cramped below chips instead of centered
3. **App icons fall back to generic** — feed URL icons aren't reliably used

---

## Issue 1 — UI Freeze (`amstoremodel.cpp`)

**Root cause:** `checkBothLoaded()` (line 316) calls `mergeDatabases()` synchronously on the main thread. The function does O(n) work over ~2200 AM apps + ~1400 feed apps: hash build, string normalization, `QIcon::hasThemeIcon()` calls per app, `updateAvailableCategories()`. Blocks 20–200ms — freezes spinner too.

**Fix:**

### Step 1 — Remove `QIcon::hasThemeIcon()` from `mergeDatabases()`

The only reason this had to stay on the main thread. Replace with direct logic:
- For merged apps with a feed icon → use `feedApp.iconSource` (HTTPS URL) directly
- For feed-only apps with HTTPS icon → keep as-is (no change needed)
- `Kirigami.Icon { source: model.iconSource }` already handles both URLs and theme names natively

```cpp
// Before (line 455-458):
if (!feedApp.iconSource.isEmpty()) {
    const QString themeKey = amApp.packageName;
    merged.iconSource = QIcon::hasThemeIcon(themeKey) ? themeKey : feedApp.iconSource;
}

// After:
if (!feedApp.iconSource.isEmpty())
    merged.iconSource = feedApp.iconSource;
```

Same change at lines 474-477 (feed-only apps block):
```cpp
// Before:
if (app.iconSource.startsWith(QStringLiteral("https://"))) {
    if (QIcon::hasThemeIcon(themeKey))
        app.iconSource = themeKey;
}

// After: remove this block entirely — keep the HTTPS URL as-is
```

### Step 2 — Make merge a static pure function

Extract `mergeDatabases()` + `updateAvailableCategories()` into a static worker:

```cpp
// amstoremodel.h — add private static:
static std::pair<QVector<StoreApp>, QStringList>
    mergeSync(QVector<StoreApp> amApps, QVector<StoreApp> feedApps);
```

The function does everything `mergeDatabases()` does today except the `QIcon::hasThemeIcon()` calls (removed in step 1). Returns `{allApps, availableCategories}`.

### Step 3 — Dispatch from `checkBothLoaded()` via `QtConcurrent::run`

```cpp
void AMStoreModel::checkBothLoaded()
{
    if (!m_amLoaded || !m_feedLoaded) return;

    auto am   = m_amApps;
    auto feed = m_feedApps;

    using Result = std::pair<QVector<StoreApp>, QStringList>;
    auto *w = new QFutureWatcher<Result>(this);
    connect(w, &QFutureWatcher<Result>::finished, this, [this, w]() {
        auto [merged, cats] = w->result();
        m_allApps              = std::move(merged);
        m_availableCategories  = std::move(cats);
        Q_EMIT availableCategoriesChanged();
        m_loading = false;
        Q_EMIT loadingChanged();
        applyFilter();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run(mergeSync, std::move(am), std::move(feed)));
}
```

Remove the old `mergeDatabases()` / `updateAvailableCategories()` body (keep `updateAvailableCategories()` called from `setStoreSource` — but its logic now lives in `mergeSync`).

**Files:** `src/gui/amstoremodel.cpp`, `src/gui/amstoremodel.h`

---

## Issue 2 — Loading Indicator Misalignment (`qml/StorePage.qml`)

**Root cause:** Lines 212-217 place `BusyIndicator` as a `ColumnLayout` sibling of the list `Item`. When the list `Item` is hidden (`visible: !amStoreModel.loading`), the `BusyIndicator` has no height to expand into — it sits pinned below the chip row.

**Fix:** Move the `BusyIndicator` inside the list `Item` with `anchors.centerIn: parent` + `z: 1`. Drop the outer `visible: !amStoreModel.loading` guard so the Item always claims its fill height.

```qml
// REMOVE lines 212-217 (standalone BusyIndicator)

// MODIFY the list Item (line 220-223):
Item {
    Layout.fillWidth: true
    Layout.fillHeight: true
    // REMOVE: visible: !amStoreModel.loading

    Controls.BusyIndicator {   // ADD
        anchors.centerIn: parent
        running: amStoreModel.loading
        visible: running
        z: 1
    }

    ListView { /* unchanged */ }
    Kirigami.PlaceholderMessage { /* unchanged */ }
}
```

Pattern mirrors `LibraryPage.qml:101-106`.

**File:** `qml/StorePage.qml`

---

## Issue 3 — Icon Fallback (resolved by Issue 1 fix)

**Root cause:** Feed URL icons were gated behind `QIcon::hasThemeIcon()`. When that returned false (most apps), `iconSource` stayed as the package name; if no matching theme icon existed, `Kirigami.Icon` showed `application-x-executable`.

**Fix:** Covered by Issue 1 step 1. After the change, merged and feed-only apps with an HTTPS icon URL always get that URL as `iconSource`. `Kirigami.Icon` loads the network image directly. No separate fix needed.

AM-only apps with no feed data and no matching theme icon will still show the generic fallback — acceptable, no external icon source exists for them.

---

## Verification

```bash
cmake --build --preset dev
sudo cmake --install build/dev
appimagemanager
```

1. Click Discover tab → spinner visible, no UI stutter, list populates smoothly
2. Spinner centered in the list area (not cramped under chips)
3. Apps with AppImage Hub data show their icons (PNG/JPG thumbnails), not the generic executable icon
4. AM-only apps (no Hub data) still show theme icon if available, else generic
5. Re-click Discover → loads instantly from cache (no second freeze)
6. Search, sort, source filter all work as before
