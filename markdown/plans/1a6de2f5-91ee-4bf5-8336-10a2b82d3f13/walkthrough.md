# Walkthrough - UI Refinements & Global Centering

I have moved the confirmation dialog to the top-level application window to ensure perfect centering and resolved the remaining UI glitches.

## Key Changes Made

### 1. Global Centering Fix
- **Moved to main.qml**: The "Confirm Deletion" dialog is now a global component in the main application window.
- **Perfect Horizontal Centering**: Because it's no longer nested inside the side pane, it now centers perfectly across the entire width of your app as requested.
- **Cross-Component Logic**: I've implemented a global `openCorpseCleanerConfirmation` function that allows the side pane to trigger this window-level dialog.

### 2. UI Stability & Tooltips
- **Fixed ToolTip Loops**: Replaced the fragile layout-based tooltips with a robust, fixed-width implementation. This stops the terminal from flooding with "Binding Loop" errors.
- **Task Indicator Fixes**: Ensured that the animated `...` and outcome icons are always reactive by fixing property assignment errors in `TaskProgressPane`.

### 3. Corpse Cleaner Refinements
- **Increased Item Visibility**: The results list is still compact (reduced height per entry), allowing you to scan many items at once.
- **Themed Borders**: The confirmation dialog now fully respects your `ui_colors.json` theme.

## How to Verify
1.  Run `python main.py`.
2.  Navigate to **Corpse Cleaner** and scan.
3.  Select any item and click **Clean Selected Corpses**.
4.  **Observe**: The confirmation box is now perfectly centered both vertically and horizontally in the middle of your screen.
5.  Hover over the system gauges at the bottom—notice the tooltips work smoothly without terminal errors.

---

# Walkthrough - Advanced Maintain System Tasks

I have successfully added the four requested advanced system maintenance tasks to the **Maintain System** module, giving you powerful interactive and automated ways to clean up deeply hidden artifacts and locks.

## Key Additions

### 1. New Automated Tasks
I've added three new command-line scripts to help stabilize and reclaim space:
- **Pacman Keyring Repair**: Uses `pacman-key` initialization, population, and refreshing to fix keyring signature errors.
- **Systemd Journal Integrity**: Runs `journalctl --verify` and forcefully vacuums logs down to 500M.
- **NVIDIA Shader Cache Purge**: Cleans `~/.cache/nvidia/GLCache` and `~/.nv/GLCache` to fix stuttering after driver updates (No root required).

### 2. Interactive EFI Boot Entry Audit (Option B)
Based on your feedback, I implemented a robust, UI-driven way to handle ghost EFI variables:
- **New Sidebar Pane**: Clicking "Audit" on the EFI Boot Entry Audit task opens a new, persistent "Ghost Entries Found" panel.
- **Safe Parsing**: It utilizes `efibootmgr -v` to read all EFI parameters safely.
- **User Control**: You can review the internal device paths, check the specific entries you want to purge, and mass-remove them securely using `pkexec` with the `-B -b` commands.
- **Consistency**: The UI matches the Corpse Cleaner, keeping the application experience perfectly cohesive.

## How to Verify
1. Run `python main.py` and navigate to the **System Maintenance** category from the sidebar or Home page.
2. Locate the "NVIDIA Shader Cache Purge" and run it to observe immediate folder deletion.
3. Locate the "EFI Boot Entry Audit" task and click the **Audit** button. 
4. Click **Scan EFI** and inspect the entries that populate.
5. Check an entry (if you are absolutely sure it is dead) and click to remove it. You will be prompted for your password via `pkexec`.

---

# Walkthrough - Stability & Threading Fix

I have resolved the `QThread` destruction crash (SIGABRT) that occurred when closing the application.

## Key Changes
- **Parented Workers**: All background `QThread` workers across the application (Command, Script, and EFI tasks) now have their parent set to the corresponding task object. This ensures they are properly cleaned up within the Qt object hierarchy.
- **Process Termination**: In `main.py`, I moved the `os._exit(0)` call to immediately follow `app.exec()`. This forces the process to terminate as soon as the main event loop closes, reliably bypassing any low-level destruction order issues that were causing the "Destroyed while thread is still running" error.

## Verification
1. Launch the application.
2. Run any task or perform a scan.
3. Close the application window.
4. **Observe**: The application now exits instantly and silently in the terminal without any Abort errors.

---

# Walkthrough - Standalone "Boot Audit" Category

As requested, I have promoted the EFI Boot entry audit to its own standalone category, named **Boot Audit**, ensuring it has its own dedicated space in the sidebar and home page.

## Changes Made
- **New Sidebar Entry**: "Boot Audit" now appears as a primary category in the navigation sidebar.
- **Direct Navigation**: Clicking the category in the sidebar or from the home page now transitions you directly to the interactive audit page, bypassing the general task selection list.
- **Renamed for Clarity**: The task is now simply called "Boot Audit" to match its new category status.
- **Integrated Icons & Strings**: The category uses a specialized "system-search" icon and has its own dedicated description in the UI string configuration.

## Verification
1. Launch the application.
2. Observe the sidebar—**Boot Audit** should now be visible below other major categories.
3. Click on **Boot Audit**.
4. **Observe**: You are taken immediately to the interactive EFI scanning page. The right-hand pane correctly shows the results area for ghost boot entries.

---

# Walkthrough - Unified Action Queue UI

I have harmonized the design of all three right-hand panes (`TaskProgressPane`, `CorpseCleanerPane`, and `EfiAuditPane`) to ensure a consistent and uniform user experience across the entire application.

## Key Harmonization Details
- **Uniform Headers**: All panes now use `Kirigami.Heading` at Level 2 for their titles, centered for optimal balance.
- **Synchronized Delegates**: Delegate layouts are now identical, featuring:
    - Dedicated columns for name and description/sub-text.
    - Standardized font weights and sizes for primary and secondary information.
    - Consistent `Kirigami.Separator` opacities (0.3) between items.
- **Visual Framing**: All list views are now enclosed in a specialized Rectangle with defined borders and background colors, matching the application's premium aesthetic.
- **Consistent Interactivity**: Hover effects and checkbox behaviors are now synchronized across all modules.

## Verification
1. Navigate between **Home**, **Corpse Cleaner**, and **Boot Audit**.
2. **Observe**: The transitions are seamless, with the right-hand pane maintaining its layout, margins, and styling regardless of the active module.
3. Compare the "Ready" list in the Home page with the results in the Corpse Cleaner—they should be visually indistinguishable in structure.

---

# Walkthrough - System Monitor Integration

I have successfully replaced the legacy persistent system gauges (SSD Health, RAM, Storage) at the bottom of the main frame with a beautifully crafted, dedicated System Monitor page inspired by KDE System Monitor.

## Key Integration Details
- **Legacy Removal**: The persistent gauges and the associated "Colorful Gauges" toggle in Settings have been fully removed.
- **Backend Refactoring (`psutil`)**: The backend Python `system_health.py` model has been completely overhauled to use the `psutil` library. It now dynamically tracks:
    - Real-time CPU, Memory, and Swap usage percentages.
    - Active network download and upload speeds, along with the active IPv4 address.
    - A dynamic list of the Top 10 most CPU-intensive running applications, tracking both CPU load and RAM allocation.
- **`SystemMonitorPage.qml`**: A brand new QML page accessible via a static link in the sidebar menu. 
    - The action queue gracefully hides when the Monitor is open.
    - Features elegant circular ring gauges drawn natively with `QtQuick.Shapes` for optimal performance.
    - Contains intuitive "cards" logically separating CPU, Memory, Swap, Drives, Networks, and the dynamic Applications table.

## Verification
1. Open the application. Note that the three gauges at the bottom of the screen are no longer present.
2. Navigate to **Settings**. Ensure the "Colorful System Gauges" switch is gone.
3. Click on the new **Monitor** icon in the sidebar.
4. **Observe**: The action queue slides out of view, and you are presented with the new System Monitor dashboard. The gauges, disks, network metrics, and the application list will organically update every 2 seconds matching the live state of your Linux machine.

---

# Walkthrough - UI Standardization (Fonts and Spacing)

I addressed the sizing discrepancies and unified the visual aesthetic of the right-hand action panes.

## Key Integration Details
- **`ui_fonts.json`**: Created a centralized global configuration file governing UI font scaling. This JSON acts similarly to the colors configuration mapping, initializing `headline`, `list_entry`, `tooltip` and `small_text` variables. 
- **`UIFontsManager`**: Modeled after the colors backend, this Python singleton loads `ui_fonts.json` and exposes it via a `UIFonts` QML alias.
- **Boot Audit Updates**: Stripped out the generic magnifying "search" icon representing Boot Audit entries. Mapped the `boot_audit.svg` custom vector graphic to the component natively.
- **Sizing Alignment**: Refactored the internal layout margins (`spacing: Kirigami.Units.smallSpacing`) across all three right-hand views (`TaskProgressPane`, `CorpseCleanerPane`, `EfiAuditPane`) to align mathematically. Bound the `Kirigami.Heading` point sizes natively to `UIFonts.fonts.headline`.

## Verification
1. Open the primary Maintainer screen. Observe the "Action Queue" headline.
2. Select "Scan Now" in the Boot Audit UI or Corpse Cleaner UI, observing their respective generated component headings.
3. Compare the "Action Queue", "Corpses Found", and "Boot Entries Found" text sizing; they should be identical and spaced equitably from the list components beneath them.

---

# Walkthrough - Settings configuration and Boot Audit Fixes

Fixed an overriding Python bug in the `TaskModel` preventing the Boot Audit queue from scanning correctly, and modernized the Scripts Path definition in user Settings.

## Key Changes
- **Boot Audit Parsing**: Injected a missing `get_task(int)` slot into `models/task_model.py`. The `EfiAuditPage.qml` controller implicitly loops through the UI model to extract its task for invocation; the lack of a bridge routine was yielding a persistent `TypeError`.
- **Boot Audit UI**: Cleaned up the `EfiAuditPane.qml` layout by permanently stripping the "Select All" and "Select None" buttons, as requested.
- **Custom Scripts Directory**: Refactored `SettingsManager.scriptsDir`. Rather than locking users out with a constant `os.path: ".../scripts"`, the property is now natively bound to `QSettings`. I mapped a new `TextField` to the UI, allowing users to safely reroute where Maintainer looks for their executables.

## Verification
1. Launch the application. Click inside "Settings".
2. Locate the overarching "Path to scripts" text field at the bottom. The default will resemble your root project `/scripts/` folder. Modify this and test closing/reopening the application to confirm `QSettings` persistance.
3. Switch over to the **Boot Audit** page. Click 'Scan'. Observe that the QML TypeError disappears and effectively runs the scanner tool natively, producing a list without the defunct "Select" buttons.

---

# Walkthrough - Boot Audit Safety Information and Confirmation

Upgraded the Boot Audit UI to transparently detail the catastrophic risks of uneducated EFI deletions, and constructed a bespoke confirmation routine mimicking Corpse Cleaner.

## Key Changes
- **Explicit Warning Banner**: Embedded a `Kirigami.Theme.negativeTextColor` bordered rectangle in `EfiAuditPage.qml` hosting a dynamic red text warning label.
- **Updated `ui_strings.json`**: Rewrote the `boot_audit.description` parameter to inform users that active OS kernels shouldn't be touched, reserving removals specifically for defunct ISO titles or ghost UUIDs. Added dialogue UI parameters such as `"confirm_title": "WARNING: Confirm Boot Entry Removal"` and `"btn_remove_now"`.
- **`efiAuditConfirmSheet`**: Implemented a standalone, specialized `Kirigami.OverlaySheet` in `main.qml` bypassing the `openCorpseCleanerConfirmation(task, items, total)` layout. It visually maps the removal queue, stripping out disk space computations (as boot sectors don't functionally reclaim significant capacity) while explicitly warning users in red that they might brick their bootloader. `EfiAuditPane.qml` now triggers this new sheet seamlessly.

## Verification
1. Open the application. Select the "Boot Audit" category.
2. Read the centralized descriptive banner. It will visually jump out highlighting the risks involved with tampering with EFI variables. 
3. Perform a "Scan EFI".
4. Checkmark an item and proceed to hit "Remove Selected Entries (1)".
5. The application will render an isolated pop-up mapping your explicit subItems. Confirm that there are no "Size Found" artifacts, and a red "Remove Entries Now" button awaits confirmation.

---

# Walkthrough - Corpse Cleaner Refresh Fix

Resolved the issue where the Corpse Cleaner list wouldn't update after cleaning by implementing a state-based UI and refining the backend refresh logic.

## Key Changes
- **State-Based UI**: Updated `CorpseCleanerPane.qml` with a `StackLayout` that manages three distinct states:
    - **Empty/Unscanned**: Shows the placeholder message.
    - **Loading/Cleaning**: Displays a `BusyIndicator` and progress message during scanning and cleaning operations.
    - **Results List**: Shows the orphan folders found.
- **Backend Refresh Logic**: Modified `GhostConfigTask` in `tasks/maintain_task.py` to explicitly clear the sub-items list upon completion and immediately trigger a re-scan. This ensures the UI is notified to hide the previous results and show the latest state.

## Verification
1. Navigate to the **Corpse Cleaner** category.
2. Click "Scan Now".
3. Select one or more items and click "Clean Selected Corpses".
4. Observe that the UI immediately switches to a cleaning/loading state.
5. Once complete, the list automatically refreshes, and the deleted items are no longer present.
