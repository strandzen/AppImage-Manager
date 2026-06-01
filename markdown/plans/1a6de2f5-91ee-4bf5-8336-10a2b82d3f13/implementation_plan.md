# Goal Description
The user reported that after cleaning corpses, the selection/list remains visible and doesn't appear to refresh. The goal is to ensure the list updates correctly after the cleaning operation is completed.

## Proposed Changes

### Backend Task Logic
#### [MODIFY] `tasks/maintain_task.py`
- In `GhostConfigTask._on_finished`, ensure that `calculate_size()` is called even on failure (or at least make it clear to the user if it failed).
- Explicitly clear sub-items and emit change signals to ensure the UI is notified immediately when cleaning finishes.

### Frontend UI Layout
#### [MODIFY] `qml/components/CorpseCleanerPane.qml`
- Implement a `StackLayout` similar to `EfiAuditPane.qml` to handle different states:
  - **Index 0**: Empty/Unscanned state (Placeholder message).
  - **Index 1**: Loading/Cleaning state (BusyIndicator + Progress message).
  - **Index 2**: Results List.
- This will provide immediate visual feedback that cleaning is in progress and ensure the list is hidden/refreshed when the operation ends.

## Verification Plan
### Manual Verification
1. Open the application and navigate to Corpse Cleaner.
2. Click "Scan Now".
3. Select some items and click "Clean Selected Corpses".
4. Verify that:
   - The UI switches to a cleaning/loading state (BusyIndicator).
   - Once finished, the list automatically refreshes and no longer shows the deleted items.
   - If no corpses remain, the "No corpses found" message appears.
