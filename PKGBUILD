pkgname=appimagemanager-git
pkgver=1.0.0
pkgrel=1
pkgdesc="A lightweight KDE Plasma utility for installing, managing, and removing AppImage files"
arch=('x86_64' 'aarch64')
url="https://github.com/YOUR_USERNAME/AppImageManager" # Update this to your repo URL
license=('GPL-2.0-or-later')
depends=(
    'qt6-base'
    'qt6-declarative'
    'kirigami'
    'kf6-coreaddons'
    'kf6-i18n'
    'kf6-config'
    'kf6-kio'
    'kf6-iconthemes'
    'kf6-notifications'
    'kf6-crash'
    'kf6-dbusaddons'
    'libappimage'
    'libcanberra'
)
makedepends=(
    'git'
    'cmake'
    'extra-cmake-modules'
    'gcc'
)
provides=('appimagemanager')
conflicts=('appimagemanager')
source=("git+https://github.com/YOUR_USERNAME/AppImageManager.git") # Update to your repo URL
sha256sums=('SKIP')

pkgver() {
    cd "$srcdir/AppImageManager"
    printf "%s" "$(git describe --long --tags | sed 's/\([^-]*-g\)/r\1/;s/-/./g')"
}

build() {
    cmake -B build -S "AppImageManager" \
        -DCMAKE_BUILD_TYPE='Release' \
        -DCMAKE_INSTALL_PREFIX='/usr' \
        -Wno-dev
    cmake --build build
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}
