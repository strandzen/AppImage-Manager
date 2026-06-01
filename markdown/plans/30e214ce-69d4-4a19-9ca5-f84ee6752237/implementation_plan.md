# CachyOS System Maintenance Tool

This document outlines the architecture and implementation steps for building the professional, minimalist system maintenance tool for Arch-based distributions (specifically CachyOS).

## Proposed Architecture

We will build the application using **C++20** with **Qt 6** and **Kirigami (QML)**. Kirigami is exactly tailored for KDE Breeze aesthetics and provides excellent components for sidebars, lists, and progressive UI states.

### 1. UI Layer (QML + Kirigami)
- **MainWindow.qml**: The root application window. Controls the window size and state transitions (Browser -> Running -> Finished).
- **Sidebar**: A Kirigami.GlobalDrawer or a custom list component that can be toggled/hidden.
- **LandingPage.qml**: Dashboard showing system health (SSD, RAM, Storage) using `ProgressBar` or similar indicators.
- **TaskPage.qml**: The view for selecting tasks, monitoring their progress, and showing the final state.

### 2. Execution Engine Layer (C++)
- **TaskEngine**: A standalone singleton or root context object that manages the queue of `BaseTask`s. It exposes the current running task, progress, and overall state to QML.
- **BaseTask**: An abstract interface defining `run()`, `isRecommended`, `name()`, `description()`, and signals like `progress()`, `finished()`, `error()`.
- **SystemMonitor**: A helper class that periodically polls or reads `/proc/meminfo`, `smartctl`, and `QStorageInfo` to feed data to the Landing Page.

### 3. Task Implementations (C++)
- **CleanTask**: Executes `pacman -Rns ...`, `paccache ...`, `journalctl --vacuum...`.
- **MaintainTask**: Executes broken symlink checks, `systemctl --failed` checks.

All C++ classes will be exposed to QML via `Q_PROPERTY` and `Q_INVOKABLE` or `QML_ELEMENT` macros for seamless reactive UI updates. Let's use `KAuth` or `pkexec` via `QProcess` for privileged execution.

## Proposed File Structure

```text
src/
├── CMakeLists.txt
├── main.cpp
├── backend/
│   ├── BaseTask.h, BaseTask.cpp
│   ├── TaskEngine.h, TaskEngine.cpp
│   ├── SystemMonitor.h, SystemMonitor.cpp
│   └── tasks/
│       ├── CleanTask.h, CleanTask.cpp
│       └── MaintainTask.h, MaintainTask.cpp
└── ui/
    ├── main.qml
    ├── LandingPage.qml
    ├── TaskPage.qml
    └── components/
        ├── SystemHealthGauge.qml
        └── TaskListItem.qml
```

## User Review Required

> [!IMPORTANT]
> Since we are in the `/home/herman/Documents/Project/Installer` folder which seems to contain Python files (`main.py`, `app_metadata.json`), should I create a completely new directory for this C++ project (e.g., `/home/herman/Documents/Project/CachyOS-Maintainer`), or should I initialize the C++ code within a specific subfolder of the current directory?

> [!NOTE]
> Could you also confirm if using Kirigami (QML) is preferred for the UI, or would you strictly prefer Qt Widgets? Kirigami perfectly aligns with the required KDE aesthetic and makes animating window shrinking/transforming states significantly easier than C++ Widgets.

## Implementation Steps

1. Scaffold the CMake project and dependencies (Qt6 Core, Gui, Qml, Quick, QuickControls2).
2. Create the robust backend C++ components (`SystemMonitor`, `TaskEngine`, `BaseTask`).
3. Connect C++ objects to the QML engine.
4. Build the QML UI components (Sidebar, Dashboard, Task lists).
5. Implement the state machine for "Transformative UI" (Browser -> Running -> Finished mode), including window resizing logic in QML.
6. Refine system commands and map `pkexec` flows.
