# AM (AppImage Package Manager) Integration Proposal

This document outlines the architectural plan, feasibility analysis, and implementation roadmap for integrating the **AM (AppMan)** package manager as a backend database and execution layer inside **AppImage Manager**.

Integrating this functionality will give AppImage Manager a first-class, native-like **App Store** experience (similar to KDE Discover or GNOME Software) for discovering, searching, downloading, and installing thousands of portable Linux applications, while keeping the UI fully integrated with the Plasma desktop.

---

## 1. Architectural Overview

The integration uses a **hybrid architecture** where:
1. **Metadata & Discovery (High Speed):** AppImage Manager pulls the application catalog directly from the raw GitHub registry using `QNetworkAccessManager` (off-thread). This provides instant search, categories, and detail views without needing to invoke slow subprocesses.
2. **Execution & Lifecycle (Pure AM Backend):** When a user clicks **Install**, **Update**, or **Remove**, AppImage Manager spawns a background `QProcess` that executes the `am` or `appman` CLI. This leverages `am`’s community-vetted shell installation recipes for downloading binaries, verifying environments, and creating launchers.
3. **Integration (KDirWatch Daemon):** Once `am` installs the application to `/opt` (system-wide) or `~/.local/share/bin` (rootless), AppImage Manager's daemon detects the new file, parses its embedded metadata, and populates the installed app dashboard automatically.

```mermaid
graph TD
    %% Define components
    subgraph Online Repository
        PL_Apps[Portable Linux Apps DB]
        AM_Repo[ivan-hc/AM Scripts]
    end

    subgraph AppImage Manager (Qt/C++)
        AM_Model[AMStoreModel]
        Net_Mgr[QNetworkAccessManager]
        Proc_Mgr[AMProcessWorker]
        Watcher[KDirWatch / Daemon]
    end

    subgraph System CLI
        AM_CLI[AM / AppMan CLI]
    end

    subgraph User Interface (QML)
        UI[Store / Discover View]
        Terminal_Dlg[Installation Console]
    end

    %% Flow lines
    Net_Mgr -- Fetches App List --> PL_Apps
    Net_Mgr -- Parses Info --> AM_Repo
    AM_Model -- Exposes Data --> UI
    UI -- Initiates Install --> Proc_Mgr
    Proc_Mgr -- Spawns Subprocess --> AM_CLI
    AM_CLI -- Executes Recipe --> System[File System /opt or ~/.local]
    System -- Triggers Event --> Watcher
    Watcher -- Updates --> UI
    Proc_Mgr -- Pipes Stdout --> Terminal_Dlg
```

---

## 2. Feasibility & Backend Mechanics

### The AM CLI Interface
The `am` (system-wide) and `appman` (local/user-level) CLIs are powerful, shell-based managers. We can invoke them natively in C++:

* **Search:** `am -q <query>` or `appman -q <query>`
* **Information:** `am -a <package_name>`
* **Installation:** `am -i <package_name>` or `appman -i <package_name>`
* **Removal:** `am -r <package_name>` or `appman -r <package_name>`
* **Update:** `am -u <package_name>`

### Bypassing CLI Parsing for Discovery (The Fast Path)
Parsing CLI output for search/discovery in a GUI is notoriously fragile and slow due to unformatted text and latency. Instead, we can fetch the database directly from the source:
* **Catalog URL:** `https://raw.githubusercontent.com/Portable-Linux-Apps/Portable-Linux-Apps.github.io/main/x86_64-appimages` (or architecture-appropriate index).
* **Format:** A clean list of supported package IDs/scripts.
* **Details URL:** Individual scripts can be fetched from `https://raw.githubusercontent.com/ivan-hc/AM/main/programs/x86_64/<package_name>` to extract direct links, license data, or repository origins.

---

## 3. Proposed QML User Interface

To align with the **KDE Kirigami Human Interface Guidelines (HIG)**, the store will be integrated as a secondary tab or secondary view in the main application.

### Store Layout Design
* **Discover Dashboard:** Hero banner of featured/recommended apps, followed by categories (Development, Games, Office, System, etc.) represented as grid sections.
* **Search Integration:** Pressing `Ctrl+F` or using the global header search dynamically switches to the store index when the "Store" tab is active.
* **App Detail Pane:**
  * Displays the app's clean name, icon (fetched online or fallback generic icon), category, and developer homepage.
  * A detailed description.
  * A clear, prominent action button: **[ Install ]** (or **[ Open ]** / **[ Remove ]** if already installed).
* **Inline Installation Console:** Spawns a sleek, slide-up panel showing a progress bar and terminal logs of the `am` installation recipe running in the background.

```
┌──────────────────────────────────────────────────────────┐
│  [  Store  ]  [ Installed ]                    [ Search] │
├──────────────────────────────────────────────────────────┤
│ Categories  │   Featured:                                │
│ > Development│   ┌────────────────────────────────────┐   │
│   Games     │   │   [Icon] Kdenlive                  │   │
│   Graphics  │   │   Professional Video Editor        │   │
│   Internet  │   │   [ Install ]                      │   │
│   Office    │   └────────────────────────────────────┘   │
│   System    │   Recent Additions:                        │
│             │   [Icon] LibreOffice   [Icon] Blender      │
└─────────────┴────────────────────────────────────────────┘
```

---

## 4. Implementation Details (C++ and QML)

To implement this without breaking the existing modular architecture, we will create a separate set of components.

### 4.1 C++ Core Extension (`src/core/`)

#### 1. `AMStoreBackend` (`amstorebackend.h / .cpp`)
Manages the online application database.
* **Responsibilities:**
  * Uses `QNetworkAccessManager` to download the `x86_64-appimages` catalog.
  * Implements local SQLite caching (either in `cache.db` or a separate `store.db` in `$XDG_CACHE_HOME/AppImageManager/`).
  * Emits `syncFinished()`, `syncFailed()`.
  * Exposes high-level searching functions that run off-thread via `QtConcurrent`.

#### 2. `AMProcessWorker` (`amprocessworker.h / .cpp`)
Handles running the installation process in the background.
* **Responsibilities:**
  * Spawns `QProcess` for `appman` (or `am`).
  * Sets up a pseudo-terminal or read-buffer to stream the stdout/stderr in real-time.
  * Emits signals: `logReceived(QString line)`, `progressUpdated(int percent)`, `finished(bool success, QString message)`.
  * Ensures processes are safe and run with appropriate local/rootless permissions.

```cpp
class AMProcessWorker : public QObject {
    Q_OBJECT
public:
    explicit AMProcessWorker(QObject *parent = nullptr);
    void installApp(const QString &packageName);
    void removeApp(const QString &packageName);

signals:
    void outputReceived(const QString &line);
    void finished(bool success, const QString &message);

private:
    QProcess *m_process;
};
```

### 4.2 GUI Backend (`src/gui/`)

#### 1. `AMStoreModel` (`amstoremodel.h / .cpp`)
A `QAbstractListModel` exposing the store catalog to QML.
* **Roles:** `packageName`, `displayName`, `category`, `description`, `homepage`, `isInstalled`, `isInstalling`.
* Handles searching, filtering by categories, and sorting.

### 4.3 QML Front-End (`qml/`)

We will introduce a new page, `StorePage.qml`, and link it into `DashboardWindow.qml`:
* **`StorePage.qml`:** Holds the search box, category list, grid delegates, and detailed view.
* **`StoreConsoleDialog.qml`:** A Kirigami Dialog showing the terminal output of `AMProcessWorker` with a neat autoscroll logger.

---

## 5. Feasibility Assessment & Potential Risks

| Risk | Mitigation Strategy |
| :--- | :--- |
| **User does not have `am`/`appman` installed** | Provide an elegant prompt on first store access to auto-install `appman` locally in rootless mode via curl (safely prompt user first). |
| **System-wide `am` requires root privilege** | Prioritize `appman` (rootless user-space mode) for desktop safety. If the system `am` is used, securely hook into `kauth` or run via standard polkit helper. |
| **Lack of detailed metadata in raw lists** | Use best-effort scraping of the `.desktop` content directly from `ivan-hc/AM` scripts or pull AppStream data from the developers' upstream repositories. |
| **Network delays on catalog search** | Cache the fetched database in SQLite locally. The catalog is updated in the background, keeping the UI 100% responsive. |

---

## 6. Implementation Stages

1. **Stage 1 (Proof of Concept):** Implement `AMStoreBackend` to query and download the `x86_64-appimages` text database, saving it locally.
2. **Stage 2 (CLI Execution Layer):** Write `AMProcessWorker` to execute `appman -i <pkg>` and pipe standard output to C++ streams.
3. **Stage 3 (Store UI Integration):** Build `StorePage.qml` and `StoreConsoleDialog.qml`, introducing a beautiful, category-based app catalog.
4. **Stage 4 (Verification & Polish):** Optimize threads, refine scroll performance, and finalize translation hooks.

---
> **Recommendation:** This integration is **100% possible** and fits perfectly into our C++/Qt/QML/Kirigami architecture. It elevates AppImage Manager from a simple desktop utility to a fully-featured, autonomous App Store for AppImages, maintaining native KDE styling and excellent performance.
