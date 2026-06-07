# v2.0 Feature Implementation

## Context

Implementing the two genuinely missing features for a 2.0 release. The other four items
identified in the "what's missing" audit (fallback icon, Ctrl+Q/W, StorePage empty state,
Hub in-app download) were already implemented in the working tree since v1.5.0.

---

## 1. Desktop file slug collision fix

**File:** `src/core/appimagemanager.cpp` — `fileKey()`

**Problem:** Two AppImages with identical `cleanName` and no reverse-domain `appId`
overwrote each other's `.desktop` and icon files silently.

**Fix:** When `appId` doesn't contain `.`, append a 6-char hex hash of `originalName`
to the slug. Apps with proper AppStream appIds are unaffected.

**Migration note:** This is a 2.0 breaking change. Pre-existing `.desktop` files for
apps without appId are orphaned (no longer detected as active by `isDesktopLinkEnabled`).
Users must re-toggle those links once. Acceptable for a major version bump.

---

## 2. GPG signature verification

### New enum: `SignatureState` (`src/core/appimageinfo.h`)

```
Unchecked=0, Valid=1, Invalid=2, Unsigned=3, GpgUnavailable=4
```

### ELF section parser (`src/core/appimagereader.cpp`)

`readElfSection(path, sectionName)` — reads ELF64 LE section headers, locates
the named section, returns its bytes. Handles the shstrtab walk for section names.

### `AppImageReader::verifySignature(path)` (static)

1. Reads `.sha256_sig` + `.sha256_hash` ELF sections (falls back to `.sha1_*`)
2. Returns `Unsigned` if no sig section found
3. Locates `gpg2` then `gpg` via `QStandardPaths::findExecutable`
4. Writes sig + hash to a `QTemporaryDir`
5. Runs `gpg --verify <sig> <hash>`, returns `Valid` or `Invalid` based on exit code

### New KConfig XT entry (`src/core/appimagemanagersettings.kcfg`)

`verifySignatures` Bool in `[Security]` group, default `false`.

### `AppSettings` (`src/core/appsettings.h/.cpp`)

`verifySignatures` Q_PROPERTY + getter/setter + signal. Emitted from `resetToDefaults()`.

### `AppImageBackend` (`src/gui/appimagebackend.h/.cpp`)

- `SigState` Q_ENUM (mirrors SignatureState, values must stay in sync)
- `beginInstall()` slot — if setting disabled: calls `doInstall()` directly;
  otherwise spawns `QtConcurrent::run(verifySignature)` and emits `signatureCheckReady(int)`
- `doInstall()` slot — performs the actual KIO install job

Old `installAppImage()` slot removed and split into these two.

### `ManageWindow.qml`

- Drag drop calls `backend.beginInstall()` (was `installAppImage()`)
- `onSignatureCheckReady(state)`: state 0/1/4 → `doInstall()`; state 2/3 → open warning dialog
- `signatureWarnDialog`: `Kirigami.Dialog` with security icon, explanation text,
  Ok (proceeds with install) / Cancel. Uses fixed `preferredWidth` to avoid binding loop.

### `SettingsPage.qml` + `SettingsDialog.qml`

`verifySignaturesCheck` FormSwitchDelegate added at top of "Behavior & Security" card.

---

## Verification

```bash
cmake --build --preset dev && sudo cmake --install build/dev
```

1. Settings → toggle "Verify GPG signatures" ON
2. Drag an unsigned AppImage to the Applications folder in ManageWindow → warning dialog appears
3. Click OK → install proceeds; Cancel → install aborted
4. Drag a valid signed AppImage → installs without dialog
5. Drag an AppImage with tampered signature → "Invalid Signature" dialog (stronger warning text)
6. Toggle OFF → all AppImages install without any sig check
7. Install two AppImages with same display name but no AppStream ID → each gets a unique
   `.desktop` file (slug has hash suffix), neither overwrites the other
