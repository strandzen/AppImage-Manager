#!/bin/bash
# Packages AppImage Manager as an AppImage (no Dolphin plugin).
# Run from the project root or from packaging/:
#   ./packaging/build-appimage.sh
#
# Requirements on the build host:
#   - All normal build dependencies (Qt6, KF6, libappimage, etc.)
#   - wget or curl (to download linuxdeploy tools if not on PATH)
#   - FUSE2 (for AppImage runtime): libfuse2 / fuse2

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build/appimage"
APPDIR="$BUILD_DIR/AppDir"
TOOLS_DIR="$BUILD_DIR/tools"

# ── Tools ────────────────────────────────────────────────────────────────────

download_tool() {
    local name="$1" url="$2"
    local dest="$TOOLS_DIR/$name"
    if [[ ! -x "$dest" ]]; then
        echo "[tools] Downloading $name..."
        mkdir -p "$TOOLS_DIR"
        wget -q --show-progress -O "$dest" "$url"
        chmod +x "$dest"
    fi
}

LINUXDEPLOY="$TOOLS_DIR/linuxdeploy"
LINUXDEPLOY_QT="$TOOLS_DIR/linuxdeploy-plugin-qt"

download_tool linuxdeploy \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
download_tool linuxdeploy-plugin-qt \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

# ── Build ────────────────────────────────────────────────────────────────────

echo "[build] Configuring..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_DOLPHIN_PLUGIN=OFF \
    -DBUILD_TESTING=OFF \
    -GNinja

echo "[build] Compiling..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "[build] Installing to AppDir..."
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"

# ── KDE QML modules ──────────────────────────────────────────────────────────
# linuxdeploy-plugin-qt bundles standard Qt QML modules but not KDE ones.
# We need: org/kde/kirigami, org/kde/kirigamiaddons, Qt.labs.platform.

QML_SYSTEM_DIR="$(qmake6 -query QT_INSTALL_QML 2>/dev/null || qmake -query QT_INSTALL_QML)"

bundle_qml_module() {
    local module_path="$1"  # e.g. "org/kde/kirigami"
    local src="$QML_SYSTEM_DIR/$module_path"
    if [[ ! -d "$src" ]]; then
        # Try KDE-specific paths (Fedora: /usr/lib64/qt6/qml, Arch: /usr/lib/qt6/qml)
        for candidate in /usr/lib64/qt6/qml /usr/lib/qt6/qml /usr/lib/x86_64-linux-gnu/qt6/qml; do
            if [[ -d "$candidate/$module_path" ]]; then
                src="$candidate/$module_path"
                break
            fi
        done
    fi
    if [[ ! -d "$src" ]]; then
        echo "[warn] QML module not found: $module_path — skipping"
        return
    fi
    local dest_dir="$APPDIR/usr/lib/qml/$module_path"
    mkdir -p "$(dirname "$dest_dir")"
    cp -r "$src" "$dest_dir"
    echo "[qml] Bundled: $module_path"
}

bundle_qml_module "org/kde/kirigami"
bundle_qml_module "org/kde/kirigamiaddons"
bundle_qml_module "Qt/labs/platform"

# ── AppRun ───────────────────────────────────────────────────────────────────

cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"

export LD_LIBRARY_PATH="$HERE/usr/lib:$HERE/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"

# Qt plugins (platforms, imageformats, iconengines, etc.)
export QT_PLUGIN_PATH="$HERE/usr/plugins:${QT_PLUGIN_PATH:-}"

# QML: system modules + bundled KDE modules + our own appimagemanager module
export QML2_IMPORT_PATH="$HERE/usr/lib/qml:$HERE/usr/lib/qt6/qml:$HERE/usr/lib64/qt6/qml:${QML2_IMPORT_PATH:-}"

# KDE icon lookup — fall back to bundled hicolor theme
export XDG_DATA_DIRS="$HERE/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

exec "$HERE/usr/bin/appimagemanager" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# ── Desktop file + icon for AppImage metadata ─────────────────────────────────
# linuxdeploy needs these at AppDir root level.
DESKTOP_SRC="$APPDIR/usr/share/applications/io.github.strandzen.AppImageManager.desktop"
ICON_SRC="$APPDIR/usr/share/icons/hicolor/scalable/apps/sc-apps-appimagemanager.svg"

if [[ -f "$DESKTOP_SRC" ]]; then
    cp "$DESKTOP_SRC" "$APPDIR/"
fi
if [[ -f "$ICON_SRC" ]]; then
    cp "$ICON_SRC" "$APPDIR/appimagemanager.svg"
fi

# ── Package ──────────────────────────────────────────────────────────────────

echo "[package] Running linuxdeploy..."
cd "$PROJECT_DIR"

QMAKE=qmake6 \
EXTRA_QT_MODULES="qml;quick;quickcontrols2;svg;dbus;network;sql;concurrent;xml" \
OUTPUT="AppImageManager-x86_64.AppImage" \
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --plugin qt \
    --output appimage

echo ""
echo "Done: $PROJECT_DIR/AppImageManager-x86_64.AppImage"
