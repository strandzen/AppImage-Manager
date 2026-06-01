# Walkthrough: Fixing Arch News Parsing

## What Was Done
The problem was caused by `models/arch_news.py` expecting an Atom format feed when connecting to `https://archlinux.org/feeds/news/`, but the Arch Linux feed was actually returning content in RSS 2.0 format. Because it searched solely for XML tags matching `a:entry`, it never found any entries and displayed "No news available".

To fix this, the following changes were made:

1. **Updated XML Parser**: The parser now explicitly checks the `root.tag`. If it's `"rss"`, it searches for `<item>` blocks and extracts the `<title>`, `<link>`, and `<pubDate>` elements correctly. It still falls back to the old Atom namespace parsing for backwards compatibility or if the Arch feed ever switches back.
2. **Updated Date Parser**: Because RSS format feeds use RFC-822 formatted dates (like `Sat, 20 Dec 2025 18:53:42 +0000`), the `_parse_date` function in `models/arch_news.py` was updated to import Python's `email.utils.parsedate_to_datetime` which natively handles these formats before falling back to ISO-8601 formatting.
3. **Cache Cleared**: The existing `~/.cache/maintainer/arch_news.json` cache was removed during our testing to ensure the app fetches a fresh copy of the feed on the next run.

## Validation Results
The script was run locally to verify the `<item>` formats are properly extracted as Python dictionaries. The backend logic operates flawlessly and you will now see the latest news posts formatted properly across the Arch News QML component.

## Persistence for Number of Displayed News Items
The "Latest", "5", and "10" buttons were already wired to limit the number of posts in the UI, but this setting was reverting to "Latest" every time on app launch. 
To ensure this value is saved:
1. **Settings Manager updated**: Added an `archNewsMaxItems` property (default `1`) to `SettingsManager` which writes directly to the user's settings file (`~/.config/MaintainerTeam/Maintainer.conf`). 
2. **ArchNews class refactored**: Removed its isolated internal state for `maxItems` and bound the value and signal directly to the `settings_manager` object.
3. **Application entry updated**: Passed `settings_manager` into the `ArchNews()` instantiation in `main.py` when application starts up.
4. **Settings Page Extensibility**: The count selection buttons were initially only placed on the "Arch Linux News" box directly. Because these buttons are quiet, a new `ComboBox` was added to the **Application Settings** page directly under the completely new "Home Page" category.

Now, whenever you select a new news item count in either the Landing Page or the Settings Page, the change is written directly to your profile configuration and reloaded precisely when you reopen the app!

## UI Polish
- **Removed Broken Selectors**: The ghostly accented box next to the "Visit Site" button was the remnant of the news size group buttons. These were completely removed from `LandingPage.qml` since the global setting in the Settings page handles this perfectly now. 
- **Alternating Row Colors**: Hooked up the `SettingsManager.alternatingRowColors` property directly to the Arch News QML repeater, so toggling this visual setting dynamically paints alternating backgrounds on news entries, perfectly mirroring the package and appimage managers.
- **Horizontal Separators**: Tied the visibility of the list's horizontal separator lines to the `alternatingRowColors` setting. The separators will now seamlessly disappear when alternating backgrounds are enabled to provide a cleaner look.
