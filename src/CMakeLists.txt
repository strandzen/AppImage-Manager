ecm_qt_declare_logging_category(LOGGING_SRCS
    HEADER logging.h
    IDENTIFIER AIM_LOG
    CATEGORY_NAME "appimagemanager"
    DEFAULT_SEVERITY Info
    EXPORT AppImageManager
    DESCRIPTION "AppImage Manager"
)

# ──────────────────────────────────────────────────────────────────────────────
# Core object library — pure logic, no Qt Quick / QML dependency.
# Linked into the QML module.
# ──────────────────────────────────────────────────────────────────────────────
kconfig_add_kcfg_files(KCFG_SRCS core/appimagemanagersettings.kcfgc)

add_library(appimagemanager_core OBJECT
    core/appimagereader.cpp
    core/appimagecache.cpp
    core/appimagemanager.cpp
    core/updatesource.cpp
    core/launchenvironment.cpp
    core/checkresults.cpp
    core/previousversion.cpp
    core/releasenotes.cpp
    core/sandbox.cpp
    ${LOGGING_SRCS}
    ${KCFG_SRCS}
)
target_include_directories(appimagemanager_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(appimagemanager_core PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    KF6::CoreAddons
    KF6::I18n
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::KIOCore
)
target_link_libraries(appimagemanager_core PUBLIC libappimage)

include(GenerateExportHeader)

# ──────────────────────────────────────────────────────────────────────────────
# QML module — contains all C++ QML_ELEMENT types AND the QML files.
# Linked by the binary and the tests.
# ──────────────────────────────────────────────────────────────────────────────
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/qml/ManageWindow.qml
    ${CMAKE_SOURCE_DIR}/qml/DashboardWindow.qml
    ${CMAKE_SOURCE_DIR}/qml/UninstallDialog.qml
    ${CMAKE_SOURCE_DIR}/qml/OnboardingDialog.qml
    ${CMAKE_SOURCE_DIR}/qml/UpdateSourceDialog.qml
    ${CMAKE_SOURCE_DIR}/qml/EnvironmentDialog.qml
    ${CMAKE_SOURCE_DIR}/qml/ConfigFolderDialog.qml
    ${CMAKE_SOURCE_DIR}/qml/SandboxDialog.qml
    ${CMAKE_SOURCE_DIR}/qml/ReleaseNotesDialog.qml
    ${CMAKE_SOURCE_DIR}/qml/AboutDialog.qml
    ${CMAKE_SOURCE_DIR}/qml/LibraryPage.qml
    ${CMAKE_SOURCE_DIR}/qml/StorePage.qml
    ${CMAKE_SOURCE_DIR}/qml/SettingsPage.qml
    ${CMAKE_SOURCE_DIR}/qml/AboutPage.qml
    ${CMAKE_SOURCE_DIR}/qml/AboutContent.qml
    ${CMAKE_SOURCE_DIR}/qml/ContributorsModel.qml
    ${CMAKE_SOURCE_DIR}/qml/TrayWindow.qml
    ${CMAKE_SOURCE_DIR}/qml/UpdateCheckToast.qml
    PROPERTIES QT_RESOURCE_ALIAS ""
)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/ManageWindow.qml
    PROPERTY QT_RESOURCE_ALIAS ManageWindow.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/DashboardWindow.qml
    PROPERTY QT_RESOURCE_ALIAS DashboardWindow.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/UninstallDialog.qml
    PROPERTY QT_RESOURCE_ALIAS UninstallDialog.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/OnboardingDialog.qml
    PROPERTY QT_RESOURCE_ALIAS OnboardingDialog.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/UpdateSourceDialog.qml
    PROPERTY QT_RESOURCE_ALIAS UpdateSourceDialog.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/EnvironmentDialog.qml
    PROPERTY QT_RESOURCE_ALIAS EnvironmentDialog.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/ConfigFolderDialog.qml
    PROPERTY QT_RESOURCE_ALIAS ConfigFolderDialog.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/SandboxDialog.qml
    PROPERTY QT_RESOURCE_ALIAS SandboxDialog.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/ReleaseNotesDialog.qml
    PROPERTY QT_RESOURCE_ALIAS ReleaseNotesDialog.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/AboutDialog.qml
    PROPERTY QT_RESOURCE_ALIAS AboutDialog.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/LibraryPage.qml
    PROPERTY QT_RESOURCE_ALIAS LibraryPage.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/StorePage.qml
    PROPERTY QT_RESOURCE_ALIAS StorePage.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/SettingsPage.qml
    PROPERTY QT_RESOURCE_ALIAS SettingsPage.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/AboutPage.qml
    PROPERTY QT_RESOURCE_ALIAS AboutPage.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/AboutContent.qml
    PROPERTY QT_RESOURCE_ALIAS AboutContent.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/ContributorsModel.qml
    PROPERTY QT_RESOURCE_ALIAS ContributorsModel.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/TrayWindow.qml
    PROPERTY QT_RESOURCE_ALIAS TrayWindow.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/UpdateCheckToast.qml
    PROPERTY QT_RESOURCE_ALIAS UpdateCheckToast.qml)

qt_add_qml_module(appimagemanager_qml
    URI "appimagemanager"
    VERSION 1.0
    QML_FILES
        ${CMAKE_SOURCE_DIR}/qml/ManageWindow.qml
        ${CMAKE_SOURCE_DIR}/qml/DashboardWindow.qml
        ${CMAKE_SOURCE_DIR}/qml/UninstallDialog.qml
        ${CMAKE_SOURCE_DIR}/qml/OnboardingDialog.qml
        ${CMAKE_SOURCE_DIR}/qml/UpdateSourceDialog.qml
        ${CMAKE_SOURCE_DIR}/qml/EnvironmentDialog.qml
        ${CMAKE_SOURCE_DIR}/qml/ConfigFolderDialog.qml
        ${CMAKE_SOURCE_DIR}/qml/SandboxDialog.qml
        ${CMAKE_SOURCE_DIR}/qml/ReleaseNotesDialog.qml
        ${CMAKE_SOURCE_DIR}/qml/AboutDialog.qml
        ${CMAKE_SOURCE_DIR}/qml/LibraryPage.qml
        ${CMAKE_SOURCE_DIR}/qml/StorePage.qml
        ${CMAKE_SOURCE_DIR}/qml/SettingsPage.qml
        ${CMAKE_SOURCE_DIR}/qml/AboutPage.qml
        ${CMAKE_SOURCE_DIR}/qml/AboutContent.qml
        ${CMAKE_SOURCE_DIR}/qml/ContributorsModel.qml
        ${CMAKE_SOURCE_DIR}/qml/TrayWindow.qml
        ${CMAKE_SOURCE_DIR}/qml/UpdateCheckToast.qml
    SOURCES
        core/appsettings.h
        core/appsettings.cpp
        core/releasechecker.h
        core/releasechecker.cpp
        gui/appimagebackend.h
        gui/appimagebackend.cpp
        gui/appimageiconprovider.h
        gui/appimageiconprovider.cpp
        gui/appimageiconstore.h
        gui/appimageiconstore.cpp
        gui/appimagelistmodel.h
        gui/appimagelistmodel.cpp
        gui/appimageupdate.h
        gui/appimageupdate.cpp
        gui/zsyncdownloader.cpp
        gui/downloadwatcher.h
        gui/downloadwatcher.cpp
        gui/appimagesortfiltermodel.h
        gui/appimagesortfiltermodel.cpp
        gui/corpsemodel.h
        gui/corpsemodel.cpp
        gui/appimagewindow.h
        gui/appimagewindow.cpp
        gui/dashboardwindow.h
        gui/dashboardwindow.cpp
        gui/amstoremodel.h
        gui/amstoremodel.cpp
        core/updatedaemon.h
        core/updatedaemon.cpp
        gui/traymodel.h
        gui/traymodel.cpp
        dbus/appimagedbusadaptor.h
        dbus/appimagedbusadaptor.cpp
    RESOURCE_PREFIX "/"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/appimagemanager"
)
generate_export_header(appimagemanager_qml
    EXPORT_MACRO_NAME APPIMAGEMANAGER_EXPORT
    EXPORT_FILE_NAME appimagemanager_qml_export.h
)

target_include_directories(appimagemanager_qml PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(appimagemanager_qml PUBLIC
    appimagemanager_core
    Qt6::DBus
    Qt6::Quick
    Qt6::Qml
    Qt6::Concurrent
    Qt6::Svg
    KF6::I18nQml
    KF6::IconThemes
    KF6::Notifications
    KF6::StatusNotifierItem
)

if(LayerShellQt_FOUND)
    # Places the tray window next to the tray on Wayland.
    target_link_libraries(appimagemanager_qml PRIVATE LayerShellQt::Interface)
    target_compile_definitions(appimagemanager_qml PRIVATE HAVE_LAYERSHELLQT)
endif()

if(Canberra_FOUND)
    target_link_libraries(appimagemanager_qml PRIVATE PkgConfig::Canberra)
    target_compile_definitions(appimagemanager_qml PRIVATE HAVE_CANBERRA)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Standalone binary — launched per-file from Open With (the .desktop MimeType),
# or directly from the command line.
# ──────────────────────────────────────────────────────────────────────────────
add_executable(appimagemanager_bin
    main.cpp
)
set_target_properties(appimagemanager_bin PROPERTIES OUTPUT_NAME "appimagemanager")
target_include_directories(appimagemanager_bin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(appimagemanager_bin PRIVATE
    appimagemanager_qml
    Qt6::Widgets
    KF6::Crash
    KF6::DBusAddons
)
install(TARGETS appimagemanager_bin DESTINATION ${KDE_INSTALL_BINDIR})
install(TARGETS appimagemanager_qml DESTINATION ${KDE_INSTALL_LIBDIR})
install(TARGETS appimagemanager_qmlplugin DESTINATION ${KDE_INSTALL_QMLDIR}/appimagemanager)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/appimagemanager/
    DESTINATION ${KDE_INSTALL_QMLDIR}/appimagemanager
    FILES_MATCHING PATTERN "qmldir" PATTERN "*.qmltypes"
)
