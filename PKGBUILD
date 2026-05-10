pkgname=appimagemanager-git
pkgver=1.2.0
pkgrel=1
pkgdesc="A lightweight KDE Plasma utility for installing, managing, and removing AppImage files"
arch=('x86_64' 'aarch64')
url="https://github.com/strandzen/AppImage-Manager"
license=('GPL-2.0-or-later')
depends=(
    'qt6-base'
    'qt6-declarative'
    'kirigami'
    'kcoreaddons'
    'ki18n'
    'kconfig'
    'kio'
    'kiconthemes'
    'knotifications'
    'kcrash'
    'kdbusaddons'
    'libappimage'
)
optdepends=(
    'kstatusnotifieritem: system tray icon support'
    'libcanberra: completion sound notifications'
    'zsync2: downloading AppImage updates'
)
makedepends=(
    'git'
    'cmake'
    'extra-cmake-modules'
    'gcc'
)
provides=('appimagemanager')
conflicts=('appimagemanager')
source=("git+https://github.com/strandzen/AppImage-Manager.git")
sha256sums=('SKIP')

pkgver() {
    cd "$srcdir/AppImage-Manager"
    printf "%s" "$(git describe --long --tags | sed 's/\([^-]*-g\)/r\1/;s/-/./g')"
}

build() {
    cmake -B build -S "AppImage-Manager" \
        -DCMAKE_BUILD_TYPE='Release' \
        -DCMAKE_INSTALL_PREFIX='/usr' \
        -Wno-dev
    cmake --build build
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}
