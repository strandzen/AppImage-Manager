cmake_minimum_required(VERSION 3.22)
project(AppImageManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(ECMInstallIcons)
include(FeatureSummary)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX APPIMAGEMANAGER
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/version.h"
)

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core
    DBus
    Gui
    Quick
    Qml
    Concurrent
    Network
)

find_package(KF6 6.0 REQUIRED COMPONENTS
    CoreAddons
    I18n
    Config
    KIO
    IconThemes
    Notifications
    Crash
    DBusAddons
)

find_package(KF6StatusNotifierItem QUIET)
if(KF6StatusNotifierItem_FOUND)
    message(STATUS "KStatusNotifierItem found — system tray support enabled")
else()
    message(STATUS "KStatusNotifierItem not found — system tray support disabled")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(Canberra IMPORTED_TARGET libcanberra)

# Optional: libappimage for in-process SquashFS metadata extraction.
# Falls back to squashfuse subprocess if not found.
find_package(libappimage QUIET)
if(libappimage_FOUND)
    message(STATUS "libappimage found — using in-process AppImage metadata extraction")
else()
    message(WARNING "libappimage not found — AppImage metadata extraction will not work. Install libappimage to enable it.")
endif()

add_subdirectory(src)
add_subdirectory(data)

ki18n_install(po)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

install(FILES data/org.kde.appimagemanager.daemon.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
