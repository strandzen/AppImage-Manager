cmake_minimum_required(VERSION 3.22)
project(AppImageManager VERSION 2.0.1 LANGUAGES CXX)

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

find_package(Qt6 6.9 REQUIRED COMPONENTS
    Core
    DBus        # session bus: daemon service registration, download dedup check
    Gui
    Quick
    Qml
    Concurrent  # QtConcurrent::run for blocking AppImageReader/findCorpses calls
    Network     # update checks via QNetworkAccessManager
    Sql         # SQLite-backed metadata cache (AppImageCache)
    Svg         # native vector icon rendering for SVGs
)

find_package(KF6 6.7 REQUIRED COMPONENTS
    CoreAddons         # KAboutData, kcoreaddons_add_plugin
    I18n               # i18n() / ki18n_install
    Config             # KSharedConfig for persistent settings
    KIO                # file install (CopyJob), trash (trash()), restore (RestoreJob)
    IconThemes         # QIcon::fromTheme / hasThemeIcon
    Notifications      # KNotification for install/update/download events
    Crash              # KCrash::initialize → DrKonqi integration
    DBusAddons         # KDBusService for multi-instance launch control
    StatusNotifierItem # daemon tray icon for pending updates
)

find_package(KF6KirigamiAddons REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(Canberra IMPORTED_TARGET libcanberra)

# libappimage for in-process SquashFS metadata extraction.
find_package(libappimage REQUIRED)

add_subdirectory(src)
add_subdirectory(data)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

ki18n_install(po)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

install(FILES data/io.github.strandzen.appimagemanager.daemon.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
