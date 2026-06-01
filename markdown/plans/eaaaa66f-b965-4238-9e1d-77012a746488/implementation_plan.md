# Build Binary with PyInstaller

## Goal
Build a standalone executable for the App Center installer using PyInstaller.

## Proposed Changes
No code changes.
Required environment change: Install `pyinstaller`.

## Verification Plan
### Automated Tests
- None.

### Manual Verification
1. Run the generated binary: `./dist/AppCenter/AppCenter` (or similar path depending on spec output).
2. Verify application launches.
