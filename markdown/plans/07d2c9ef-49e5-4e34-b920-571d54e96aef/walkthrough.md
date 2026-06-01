# Update System Integration Walkthrough

I have integrated a robust update system into the App Center. This system handles both data updates (package lists) and application binary updates (AppImage).

## Core Enhancements

### 1. Data Versioning
The `packages.json` file now includes a `version` field. This allows the app to compare its local data with the latest version available on GitHub.

```json
{
    "version": "1.0.0",
    "system": { ... }
}
```

### 2. Remote Update Logic
I've added a new `Updater` class in [logic.py](file:///home/herman/Documents/Project/Installer/logic.py) that:
- Fetches the latest `packages.json` from GitHub.
- Checks the GitHub Releases API for new AppImage tags.
- Safely compares semantic versions (e.g., `1.0.1` > `1.0.0`).

### 3. Persistent Data Storage
To allow data updates without rebuilding the AppImage, updated package lists are stored in `~/.config/app-center/packages.json`. The application automatically prioritizes this file over the bundled one if it exists and is valid.

### 4. Background Update Checks
The [MainWindow](file:///home/herman/Documents/Project/Installer/gui.py) now performs asynchronous update checks on startup:
- **Data Updates**: If a new `packages.json` is found, the user is prompted to download it.
- **App Updates**: If a new AppImage release is found, a notification with a download link is shown.

## Technical Details

- **Workers**: I implemented `DataUpdateWorker` and `AppUpdateWorker` using `QThread` to ensure the UI remains responsive during network calls.
- **Fail-safe**: The system handles network errors and 404s (e.g., if the repo is empty) gracefully without affecting app startup.

### 5. Structured Previews
The `previews` directory has been reorganized for better maintainability:
- All `.md` files are now in `previews/Markdown/`.
- All `.png` files are now in `previews/Images/`.
- The `get_preview_files` method in [logic.py](file:///home/herman/Documents/Project/Installer/logic.py) was updated to discover files in these specific subdirectories efficiently.

### 6. UI & Logic Refinements
I've fine-tuned the user experience based on your feedback:
- **Consolidated Update Center**: Manual update checks now trigger a single, unified dialog. It displays the status of the App Center binary (with download links), the Package List (with an update button), and a detailed list of available updates for managed applications (Flatpaks/Apt).
- **Progress Feedback Redesign**: Installations and updates now use a progress-bar-focused view by default. Users can toggle a "Show Details" button to reveal the live console output if they need to see technical progress.
- **Hidden Simulation Category**: I've added a dedicated "Hidden" category. Its entries are hidden by default and can be revealed by searching for **"hidden"**. This includes a safe "Simulation" entry for testing the UI.
- **Branded App Previews**: All app previews now feature a high-resolution icon (256x256) aligned to the left of the title, creating a more professional and modern appearance.
- **Pinned Release Installation**: Updated `Theme-Switcher` to install from the stable `Release` tag instead of `latest`, ensuring predictable versioning.
- **ID-Based Resource Discovery**: The preview system has been standardized to use Flathub IDs for file naming (e.g., `com.bitwarden.desktop.md`). This eliminates discovery errors caused by localized names or status suffixes, making the resource loading instant and robust.
- **Visual App Sidebar**: I've integrated app icons directly into the list view sidebar. Each entry now displays its professional icon (24x24) next to the name, significantly improving navigation and visual consistency throughout the app.
- **Automated Resource Integration**: The app now automatically manages assets by instantaneously discovering local icons and descriptions based on the Flathub ID. This eliminates manual file linking while keeping the application offline-capable and fast.
- **Lowercase ID Standardization**: Standardized the entire project to use lowercase Flathub IDs in both `packages.json` and file resources. This ensures seamless compatibility on case-sensitive file systems.
- **Pascal Case Naming & Extracted Metadata**: Introduced `app_metadata.json` as a centralized source for app display names (using Pascal Case with spaces, e.g., `Brave Browser`) and GUI settings. This allows for easy editing of names and parameters like `sidebar_icon_size` without touching the core logic.
- **Alphabetical Sorting**: All apps are now automatically sorted alphabetically within their respective categories, making the installer significantly easier to browse.
- **Context-Aware Styling**: The application dynamically updates item styling based on the active tab.

### Visual Context
The following screenshots provided during development show the "Install" vs "Manage" states that were refined:

````carousel
![Install Tab View (with strikeouts)](/home/herman/.gemini/antigravity/brain/07d2c9ef-49e5-4e34-b920-571d54e96aef/uploaded_image_0_1769281804684.png)
<!-- slide -->
![Manage Tab View (Clean)](/home/herman/.gemini/antigravity/brain/07d2c9ef-49e5-4e34-b920-571d54e96aef/uploaded_image_1_1769281804684.png)
````

## Verification Results

- Verified that `SystemScanner` correctly loads and prioritizes `~/.config/app-center/packages.json`.
- Verified that the `Updater` class correctly identifies version differences.
- Verified that UI transitions between tabs correctly update the display of application items.
- Verified that update checks are successfully triggered from the "Manage" tab.
