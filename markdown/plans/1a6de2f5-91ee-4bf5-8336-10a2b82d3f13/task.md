# TaskTracker: Corpse Cleaner Refresh

## Planning
- [x] Investigate `GhostConfigTask` and `CorpseCleanerPane.qml`.
- [x] Draft `implementation_plan.md`.
- [x] Get user review.

## Frontend UI (`CorpseCleanerPane.qml`)
- [x] Implement `StackLayout` for state-based UI (Empty, Loading/Cleaning, Results).
- [x] Integrate `BusyIndicator` for cleaning/scanning states.

## Backend Logic (`tasks/maintain_task.py`)
- [x] Ensure `_on_finished` triggers a refresh regardless of success.
- [x] Verify `calculate_size` properly signals the UI.

## Verification
- [x] Execute `python main.py`.
- [x] Perform a clean and verify the UI refreshes and shows a loading state during the operation.
- [x] Verify the list is cleared correctly after cleaning.
