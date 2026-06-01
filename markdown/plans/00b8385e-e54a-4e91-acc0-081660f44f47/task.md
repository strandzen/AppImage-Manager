# Task Breakdown

- [x] Refactor project structure (`models/`, `tasks/`, `utils/`)
- [x] Implement `models/system_health.py` (RAM, Storage, SSD)
- [x] Implement `utils/privileged_process.py` (pkexec wrapper)
- [x] Implement `tasks/base_task.py`
- [x] Refactor `tasks/clean_task.py` and `tasks/maintain_task.py`
- [x] Implement `models/task_engine.py` and `models/task_model.py`
- [x] Update `main.py` to create `cleanTaskModel` and `maintainTaskModel`
- [x] Update `TaskSelectionPage.qml` to accept dynamic model and title
- [x] Test system locally
## Version 2: Advanced Diagnostics and Maintenance

- [x] **Landing Page System Health**
  - [x] Implement smartctl integration for SSD temperature and health.
  - [x] Update Storage Overview to use PyQt6 `QStorageInfo` for `/` and `/home`.
- [x] **Clean Module**
  - [x] Add Package Cache clearing to config
  - [x] Add Journald Vacuuming to config
  - [x] Add Unused Flatpak Runtimes to config (`flatpak uninstall --unused`)
  - [x] Implement custom `UserCacheTask` for selective `~/.cache` clearing
- [x] **Maintain Module**
  - [x] Implement custom `GhostConfigTask` (audit `~/.config` vs `pacman -Qq`)
  - [x] Add Broken Symlink Finder to config
  - [x] Add Systemd Service Health check to config
  - [x] Add Pacman Lock-file Repair to config
  - [ ] Implement `DiskHealthTask` for active S.M.A.R.T diagnostic report
- [x] **Settings Module**
  - [x] Create `settings_manager.py` using PyQt6 `QSettings`
  - [x] Create `SettingsPage.qml` and connect to sidebar footer
  - [x] Bind settings (log age, cache count) to dynamic tasks
- [x] **Task Engine Enhancements**
  - [x] Add `calculate_size()` asynchronous method to `BaseTask` and `CommandTask`
  - [x] Add `reclaimedSpace` property to QML `TaskModel`
  - [x] Update `TaskSelectionPage.qml` delegate to display reclaimed space

## Version 3: Visual Identity & Polishing

- [x] **Icon Externalization (Local Assets)**
  - [x] Create `icons/` directory
  - [x] Generate Category Icons (Clean, Maintain, Settings, Home)
  - [x] Generate State Icons (Success, Error, Warning)
  - [x] Update QML files to use local icon sources
  - [x] Update `tasks_config.json` for category icon paths

- [ ] **Navigation & UX Refinement**
  - [x] Add 'Return to Home' button on FinishedTasksPage
  - [ ] Set local hamburger menu icon for drawer handle
