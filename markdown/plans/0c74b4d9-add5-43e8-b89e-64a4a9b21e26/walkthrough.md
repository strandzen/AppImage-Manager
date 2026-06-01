# Walkthrough: Profile Saving Feature

I have replaced the "Restart Plasma" button with a "Save current" button. This new feature allows you to save your current KDE configuration as a Konsave profile directly from the application.

## Changes Made

### Konsave Application

#### [themeSwitcher](file:///home/herman/Documents/Project/Konsave/themeSwitcher)

- **Removed "Restart Plasma"** button and its associated logic.
- **Added "Save current"** button.
- **Implemented `save_current_action`**:
    - Prompts for a profile name using a standard KDE-style input dialog.
    - Checks if the name already exists in `~/.config/konsave/profiles`.
    - Asks for confirmation before overwriting an existing profile.
    - Executes `konsave -s <name>` (adding `--force` if overwriting).
    - Generates a `README.md` file inside the new profile folder with the text: "Custom profile, saved by user".
    - Automatically refreshes the profile list to show the new profile.

## Verification Results

### Manual Verification
- **UI Update**: The "Restart Plasma" button is replaced by "Save current".
- **Saving Logic**:
    - Prompt appears correctly.
    - `konsave` command executes as expected.
    - `README.md` is created in the correct path.
    - List refreshes immediately after saving.
- **Overwrite Protection**: The confirmation dialog correctly triggers the `--force` flag when "Yes" is selected.

render_diffs(file:///home/herman/Documents/Project/Konsave/themeSwitcher)
