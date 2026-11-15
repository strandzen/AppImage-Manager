<div align="center">
    <img width="225" height="225" src="data/sc-apps-appimagemanager.svg">
    <h1><b>AppImage Manager</b></h1>
</div>

Install, update and remove AppImages on KDE Plasma 6.

Drop an AppImage on the window, or right-click it in Dolphin, and it goes into one Applications folder and, if you like, into your app menu. AppImage Manager checks your AppImages for updates, can find new apps in online catalogs, and cleans up after the apps you remove.

It doesn't care where your AppImages come from. Updates work from the information built into an AppImage, or from a GitHub, Codeberg, Forgejo or Gitea repository, a zsync file or a plain download link that you set yourself. The app catalogs are optional, and you can add your own.

This is version 2.1.0. I mostly test it on my own machine on Arch, so other distributions may need some work.

> [!NOTE]
> This project built with heavy AI assistance. It is an independent project and is not affiliated with or endorsed by KDE or KDE e.V.

## Screenshots - Coming Soon

## Features

- Install an AppImage by dragging it onto the window, with the + button, or from Dolphin's right-click menu. AppImages in Downloads are moved, others (from a USB stick, say) are copied.
- Add an app to your app menu, or take it out again, with one click.
- All your AppImages in one list, with their icon, version, size, categories and description. Search, sort by name, size, category or date added, and see them grouped when sorted by category or date.
- Check all AppImages for updates at once, then update one or all of them. Updates use zsync when `zsync2` is installed, so only the changed parts are downloaded.
- Set where an AppImage is released when it doesn't say so itself: paste a repository, a zsync link or a download link and the kind is detected. Pre-releases are optional, the file for your CPU is picked for you, and a download that turns out to be a web page is refused.
- Environment variables per app, for example `WEBKIT_DISABLE_DMABUF_RENDERER=1` for an app that shows a blank window. They apply when you start the app from AppImage Manager and from the app menu.
- Background update checks with a tray icon and notifications, daily, weekly, monthly or every few days.
- A notification when an AppImage lands in your Downloads folder, with a button to manage it.
- Discover: browse and install apps from AppImage Hub and the AM database, or your own catalogs. It's off until you turn it on, so nothing is downloaded from catalogs unless you want it.
- Remove an app and choose which of its leftover settings and cache folders go with it. Everything goes to the Trash, nothing is deleted outright.
- Optional signature check before installing: you're warned about unsigned AppImages and ones with a broken signature.
- Looks and behaves like a Plasma app and follows your theme.

## Requirements

- KDE Plasma 6 (Qt 6.9 or newer and KDE Frameworks 6).
- `libappimage`, to read AppImages without mounting them.
- Optional: `zsync2` for smaller update downloads, `gpg` for signature checks, and [am](https://github.com/ivan-hc/AM) to install apps from the AM database in Discover.

## Installation

### Arch Linux

Install from the AUR, for the latest release:

```sh
paru -S appimage-manager-plasma
```

Or `appimage-manager-plasma-git` for the latest development version. Your AUR helper also installs `libappimage`, which is only in the AUR.

To build the source you have checked out, as a package you can remove again with pacman, run this in `packaging`:

```sh
makepkg -sif
```

The `-f` matters: without it, makepkg reinstalls a package it built earlier instead of building your changes.

### Other distributions

These aren't tested yet. Install the build dependencies (see below), then:

```sh
cmake --preset release
cmake --build --preset release
sudo cmake --install build/release
```

<details>
<summary><strong>Build dependencies</strong></summary>

Arch:

```sh
sudo pacman -S base-devel cmake extra-cmake-modules ninja \
    qt6-base qt6-declarative qt6-svg \
    kcoreaddons ki18n kconfig kio kiconthemes knotifications kcrash kdbusaddons \
    kirigami kirigami-addons kstatusnotifieritem gnupg
paru -S libappimage        # optional: zsync2 am-git
```

Fedora:

```sh
sudo dnf install gcc-c++ cmake extra-cmake-modules ninja-build \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel \
    kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kconfig-devel kf6-kio-devel kf6-kiconthemes-devel \
    kf6-knotifications-devel kf6-kcrash-devel kf6-kdbusaddons-devel kf6-kstatusnotifieritem-devel \
    kf6-kirigami-devel kf6-kirigami-addons-devel libappimage-devel gnupg2
```

Debian and Ubuntu (24.04 or newer):

```sh
sudo apt install build-essential cmake extra-cmake-modules ninja-build \
    qt6-base-dev qt6-declarative-dev libqt6svg6-dev \
    libkf6coreaddons-dev libkf6i18n-dev libkf6config-dev libkf6kio-dev libkf6iconthemes-dev \
    libkf6notifications-dev libkf6crash-dev libkf6dbusaddons-dev libkf6statusnotifieritem-dev \
    qml6-module-org-kde-kirigami qml6-module-org-kde-kirigamiaddons \
    libappimage-dev gnupg2
```

openSUSE Tumbleweed:

```sh
sudo zypper in gcc-c++ cmake extra-cmake-modules ninja \
    qt6-base-devel qt6-declarative-devel qt6-svg-devel \
    kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kconfig-devel kf6-kio-devel kf6-kiconthemes-devel \
    kf6-knotifications-devel kf6-kcrash-devel kf6-kdbusaddons-devel kf6-kstatusnotifieritem-devel \
    kf6-kirigami-devel kf6-kirigami-addons-devel libappimage-devel gpg2
```

</details>

### After installing

Restart Dolphin to get **Manage AppImage** in its right-click menu:

```sh
kquitapp6 dolphin && dolphin &
```

## Usage

The first time you open AppImage Manager, one page asks for your Applications folder, whether to notify you about downloads, and whether to turn on Discover. You can change all of it later in Settings.

- **Install:** drag an AppImage onto the window or the drop box under the list, click +, or right-click the file in Dolphin and choose **Manage AppImage**.
- **Update:** open the menu (☰) and choose **Check for Updates**. Apps with an update get a dot and an **Update to …** button; with several, **Update All** appears in the toolbar.
- **More options:** select an app and click ⋯ for **Show in App Menu**, **Update Source…**, **Environment Variables…** and **Remove…**.

### Keys

| Key | What it does |
|---|---|
| Ctrl+F | Search |
| Enter | Start the selected app |
| Delete | Remove the selected app |
| Esc | Clear the search, then the selection |
| Ctrl+, | Settings |
| Alt+Left | Back from Settings or About |
| Ctrl+Q | Quit |

### From the command line

| Command | What it does |
|---|---|
| `appimagemanager` | Opens the main window |
| `appimagemanager /path/to/app.AppImage` | Opens the install window for that file |
| `appimagemanager --daemon` | Runs the background update checker (starts at login by itself) |

## Settings

Open them from the menu (☰), or with Ctrl+,.

- **General:** the Applications folder, the drop box under the list, and notifications
- **Updates:** how often to check in the background
- **Security:** signature checks and the reminder that AppImages aren't verified
- **Discover:** turn it on or off
- **Catalogs:** which app catalogs Discover searches, in which order, and your own

**Restore Defaults** is at the bottom.

## Troubleshooting

**An app shows a blank or broken window.** Many apps built with WebKit or Electron need an environment variable on some systems. Select the app, click ⋯ → **Environment Variables…** and add, for example, `WEBKIT_DISABLE_DMABUF_RENDERER=1`.

**An AppImage never gets updates.** It probably doesn't say where it is released. Click ⋯ → **Update Source…** and paste its repository or download link. The line under the field shows what was detected; pick another type there if it guessed wrong.

**I rebuilt with makepkg but nothing changed.** Use `makepkg -sif`, see [Arch Linux](#arch-linux). Then close AppImage Manager completely and start it again: a window that is still open just comes to the front.

**pacman says files already exist.** They came from an earlier `sudo cmake --install`. Remove them with `sudo xargs rm < build/release/install_manifest.txt` (or `build/dev/…`), then install again.

**I want to see what AppImage Manager is doing.** Turn on its debug output in `kdebugsettings` (search for "appimagemanager"), or start it from a terminal with:

```sh
QT_LOGGING_RULES="appimagemanager.debug=true" appimagemanager
```

Bugs and ideas are welcome in the [issue tracker](https://github.com/strandzen/AppImage-Manager/issues).

## Uninstall

With the package: `sudo pacman -R appimage-manager-plasma` (or `appimage-manager-plasma-git`). With CMake, in the source folder:

```sh
sudo xargs rm < build/release/install_manifest.txt
```

Your AppImages stay in your Applications folder. Remove app menu entries you don't want first (⋯ → **Show in App Menu**), or delete the `appimage_*.desktop` files in `~/.local/share/applications`.

## Translations

AppImage Manager is translated into Czech, Dutch, French, German, Italian, Japanese, Norwegian (Bokmål), Polish, Portuguese (Brazil), Russian, Simplified Chinese, Slovak, Spanish, Turkish and Ukrainian. Text added in 2.1.0 isn't translated yet. To add or improve a language, edit `po/<language>/appimagemanager.po`, or copy `po/appimagemanager.pot` for a new one.

## License

GPL-2.0-or-later. See [LICENSES/GPL-2.0-or-later.txt](LICENSES/GPL-2.0-or-later.txt).
