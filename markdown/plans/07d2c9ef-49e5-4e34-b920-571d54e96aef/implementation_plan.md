# Automatic Resource Integration Plan

This plan details the automation of app asset discovery (icons, images, descriptions) and the implementation of remote fallbacks for missing local resources.

## Proposed Changes

### Logic Tier (`logic.py`)

#### [MODIFY] [logic.py](file:///home/herman/Documents/Project/Installer/logic.py)
- **New Method `get_app_resources(self, app_id)`**:
    - Centralize discovery of `icon`, `image` (preview), and `markdown` (description).
    - Always return absolute paths if they exist locally.
- **Improved Fallback Logic (Flathub API v2)**:
    - If a local icon or description is missing, utilize `https://flathub.org/api/v2/appstream/{app_id}`.
    - Extract `description` (HTML/Text) and `icon` (URL) from the JSON response.
    - Cache remote icon URLs to prevent redundant API calls.

### GUI Tier (`gui.py`)

#### [MODIFY] [gui.py](file:///home/herman/Documents/Project/Installer/gui.py)
- **Refactor `update_item_display`**: Use `scanner.get_app_resources()` to set the sidebar icon.
- **Refactor `on_item_selected`**:
    - Use `scanner.get_app_resources()` for the preview panel.
    - **Asset Fetching**: If local assets are missing, use a background worker to fetch the Flathub metadata/images.
- **New `ResourceWorker`**:
    - A dedicated worker to fetch icons/descriptions from remote sources (Flathub) for apps that lack local assets.

## Verification Plan

### Automated/Manual Verification
1. **Local Discovery**: Add a new app ID to `packages.json` and place matching named files in `previews/`. Verify they appear instantly.
2. **Standard Fallback**: Add an app ID that exists on Flathub but has no local files. Verify that the app eventually displays its Flathub icon and description in the UI.
3. **Robustness**: Ensure the UI remains responsive while remote assets are being fetched.
