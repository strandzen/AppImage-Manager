# Maintainer Project: App Overview & Current State

The **Maintainer** app is a specialized system maintenance utility for Linux, built with **Python (PyQt6)** and **QML (Kirigami)**. It provides a centralized dashboard for monitoring system health, executing maintenance tasks, and managing AppImages.

## 🚀 Core Features

### 1. System Health Monitoring
- Real-time gauges for **SSD Wear/Health**, **RAM Usage**, and **Storage Capacity**.
- Persistent health overview available across the main landing and system monitor pages.

### 2. Task Management Engine
- **Task Registry**: A flexible system for defining maintenance tasks (shell scripts or Python logic) via JSON.
- **Favorites & Recommended**: Intelligent tagging of tasks as "Recommended" based on system state.
- **Sequential Execution**: A robust `TaskEngine` that handles task queues, progress reporting, and output logging.

### 3. AppImage Manager
- **Scanning**: Automatically detects `.AppImage` files in a configurable directory.
- **Update Integration**: Links AppImages to GitHub repositories to check for the latest releases.
- **Smart Updates**: In-app downloading and replacement of outdated AppImages with automatic permission handling (`chmod +x`).
- **Batch Management**: Support for multi-selection and batch deletion of AppImages.

### 4. Advanced Maintenance Tools
- **Corpse Cleaner v2**: A sophisticated tool to find and remove leftover configuration and data files from uninstalled applications.
- **EFI/Boot Audit**: Insights into EFI variables and boot configuration health.

### 5. Dynamic Design System
- **JSON-Driven UI**: Entire look and feel (colors, fonts, icons, strings) is configurable via external JSON files.
- **SVG Icon System**: Scalable, colorizable SVG icons with fine-grained control over overrides.
- **Kirigami Framework**: Uses KDE's Kirigami for a modern, responsive, and native-feeling Linux experience.

---

## 📊 Current Integration Status

| Component | Status | Description |
| :--- | :--- | :--- |
| **AppImage UI** | 🟢 Almost Complete | Detailed center pane and side panel integration are functional. Download progress and state-driven buttons are implemented. |
| **Task Engine** | 🟢 Stable | Handles sequential execution and output capture reliably. |
| **UI Config** | 🟢 Stable | Icon coloring, font scaling, and theme management are fully operational. |
| **Corpse Cleaner** | 🟡 Refocussed | Recently fixed refresh issues; UI provides clear feedback during cleaning. |

## 🛠 Next Steps (Planned)
- implement **"Update Checked"** in AppImage Manager to handle multiple updates at once.
- Refine **Landing Page** layout to further unify the "Maintainer" branding.
- Expand **Task Registry** with more built-in Linux maintenance scripts.
