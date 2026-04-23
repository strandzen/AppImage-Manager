cmake_minimum_required(VERSION 3.22)
project(AppImageManager VERSION 0.1.0 LANGUAGES CXX)

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
    Gui
    Quick
    Qml
    Concurrent
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

find_package(PkgConfig REQUIRED)
pkg_check_modules(Canberra IMPORTED_TARGET libcanberra)

# Optional: libappimage for in-process SquashFS metadata extraction.
# Falls back to squashfuse subprocess if not found.
find_package(libappimage QUIET)
if(libappimage_FOUND)
    message(STATUS "libappimage found — using in-process AppImage metadata extraction")
else()
    message(STATUS "libappimage not found — using squashfuse subprocess (install libappimage for better performance)")
endif()

add_subdirectory(src)
add_subdirectory(data)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
