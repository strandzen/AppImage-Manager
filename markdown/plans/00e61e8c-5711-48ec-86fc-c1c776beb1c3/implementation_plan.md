# Integrate Theme-Switcher

Integrate the Theme-Switcher installation script into the `SystemScanner.generate_install_plan` workflow, ensuring it runs before the Affinity installation as requested.

## Proposed Changes

### Component List Sorting & Order Preservation

#### [MODIFY] [gui.py](file:///home/herman/Documents/Project/Installer/gui.py)
- Update `add_task_item` to accept and store an `original_index` in the item's data.
- Update `init_ui` to:
    1. Sort the tasks fetched from the scanner.
    2. Sorting criteria: Non-flathub items first (alphabetical), then Flathub items (alphabetical).
    3. Add items to the list widget with their original indices from `self.tasks`.
- Update `add_flatpak` to assign a default index (e.g., 4.5) to custom Flatpaks so they install after standard Flatpaks.
- Update `start_installation` to:
    1. Collect all checked items.
    2. Sort them by their stored `original_index` to ensure the correct installation sequence.
    3. Pass the correctly ordered task list to the `TerminalWindow`.

## Verification Plan

### Automated Tests
- Syntax check.
- Verify sorting logic manually by running the app.

### Manual Verification
- Run the installer.
- Verify the list is sorted: System/Other first, then (flathub) items.
- Verify selection works and installation happens in the correct order (e.g., updates first, then apps, then Affinity last).
