# Task Checklist

- [x] Investigate Arch News parsing failure
- [x] Identify feed format change (Atom to RSS)
- [x] Update XML parsing logic in `models/arch_news.py` to handle RSS 2.0 format
- [x] Update date parsing logic in `models/arch_news.py` to handle `pubDate` format (`%a, %d %b %Y %H:%M:%S %z`)
- [x] Clear cached file (`~/.cache/maintainer/arch_news.json`) to force a fresh fetch
- [x] Verify fix by running application logic


# Fix Arch News Selection Persistence
- [x] Add `archNewsMaxItems` property to `SettingsManager`
- [x] Bind `ArchNews`'s `maxItems` to `SettingsManager`
- [x] Pass `settings_manager` object to `ArchNews` inside `main.py`
- [x] Verify settings persist correctly
- [x] Add obvious visual combo box to `SettingsPage.qml` under "Home Page" category

# Arch News UI Polish
- [x] Remove the broken/invisible quick select buttons from the Landing Page that look like a blank square
- [x] Apply `alternatingRowColors` background styling to the Arch News `ListView` / `Repeater`
- [x] Hide horizontal separators between rows when `alternatingRowColors` is enabled
