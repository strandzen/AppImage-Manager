# Execution Task List

- [x] Fast Startup
  - [x] Implement `lru_cache` for `get_appimage_info` in `core.py`.
  - [x] Modify `is_desktop_link_enabled` to use `_get_app_id` or string manipulation instead of `get_appimage_info`.
- [x] No more UI Freezing (Multithreading)
  - [x] Run `get_appimage_info` asynchronously in `gui.py` `__init__`. Add loading state to UI.
  - [x] Run `find_app_corpses` asynchronously.
  - [x] Run `remove_items` asynchronously.
- [x] Trash Deletion
  - [x] Use `kioclient move <file> trash:/` in `remove_items` instead of `shutil.rmtree` and `os.remove`.
- [x] Restore Icon Logic & Prioritize System Theme
  - [x] Restore robust icon search logic in `core.py`.
  - [x] Update `appIconSource` in `gui.py` to prioritize `QIcon.hasThemeIcon`.
  - [x] Update `.desktop` file generation to use `Icon=<app_id>`.
- [x] Exec Sanitization
  - [x] Replace regex with `shlex.split`.
- [/] Update `README.md`
  - [ ] Add note about Type 2 AppImages.
  - [ ] Include all recent progress.
