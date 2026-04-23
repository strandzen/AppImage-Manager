ecm_qt_declare_logging_category(LOGGING_SRCS
    HEADER logging.h
    IDENTIFIER AIM_LOG
    CATEGORY_NAME "org.kde.appimagemanager"
    DEFAULT_SEVERITY Info
    EXPORT AppImageManager
    DESCRIPTION "AppImage Manager"
)

# ──────────────────────────────────────────────────────────────────────────────
# Core object library — pure logic, no Qt Quick / QML dependency.
# Both the plugin and the binary link against this.
# ──────────────────────────────────────────────────────────────────────────────
add_library(appimagemanager_core OBJECT
    core/appimagereader.cpp
    core/appimagemanager.cpp
    ${LOGGING_SRCS}
)
target_include_directories(appimagemanager_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(appimagemanager_core PUBLIC
    Qt6::Core
    KF6::CoreAddons
    KF6::I18n
    KF6::ConfigCore
    KF6::KIOCore
    KF6::KIOWidgets
)
if(libappimage_FOUND)
    target_link_libraries(appimagemanager_core PUBLIC libappimage)
    target_compile_definitions(appimagemanager_core PUBLIC HAVE_LIBAPPIMAGE)
endif()

include(GenerateExportHeader)

# ──────────────────────────────────────────────────────────────────────────────
# QML module — contains all C++ QML_ELEMENT types AND the QML files.
# Linked by both the plugin and the binary.
# ──────────────────────────────────────────────────────────────────────────────
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/qml/ManageWindow.qml
    ${CMAKE_SOURCE_DIR}/qml/UninstallWindow.qml
    PROPERTIES QT_RESOURCE_ALIAS ""
)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/ManageWindow.qml
    PROPERTY QT_RESOURCE_ALIAS ManageWindow.qml)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/UninstallWindow.qml
    PROPERTY QT_RESOURCE_ALIAS UninstallWindow.qml)

qt_add_qml_module(appimagemanager_qml
    URI "org.kde.appimagemanager"
    VERSION 1.0
    QML_FILES
        ${CMAKE_SOURCE_DIR}/qml/ManageWindow.qml
        ${CMAKE_SOURCE_DIR}/qml/UninstallWindow.qml
    SOURCES
        gui/appimagebackend.h
        gui/appimagebackend.cpp
        gui/appimageiconprovider.h
        gui/appimageiconprovider.cpp
        gui/corpsemodel.h
        gui/corpsemodel.cpp
        gui/appimagewindow.h
        gui/appimagewindow.cpp
    RESOURCE_PREFIX "/"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/org/kde/appimagemanager"
)
generate_export_header(appimagemanager_qml
    EXPORT_MACRO_NAME APPIMAGEMANAGER_EXPORT
    EXPORT_FILE_NAME appimagemanager_qml_export.h
)

target_include_directories(appimagemanager_qml PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(appimagemanager_qml PUBLIC
    appimagemanager_core
    Qt6::Quick
    Qt6::Qml
    Qt6::Concurrent
    KF6::I18nQml
    KF6::IconThemes
    KF6::Notifications
)

if(Canberra_FOUND)
    target_link_libraries(appimagemanager_qml PRIVATE PkgConfig::Canberra)
    target_compile_definitions(appimagemanager_qml PRIVATE HAVE_CANBERRA)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Dolphin plugin — KAbstractFileItemActionPlugin (.so)
# Auto-discovered by Dolphin via plugin JSON metadata.
# ──────────────────────────────────────────────────────────────────────────────
kcoreaddons_add_plugin(appimagemanager
    SOURCES plugin/appimageactionplugin.cpp
    INSTALL_NAMESPACE "kf6/fileitemaction"
)
target_include_directories(appimagemanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(appimagemanager PRIVATE appimagemanager_qml)

# ──────────────────────────────────────────────────────────────────────────────
# Standalone binary — launched per-file from the service menu,
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
    KF6::Crash
    KF6::DBusAddons
)
install(TARGETS appimagemanager_bin DESTINATION ${KDE_INSTALL_BINDIR})
install(TARGETS appimagemanager_qml DESTINATION ${KDE_INSTALL_LIBDIR})
install(TARGETS appimagemanager_qmlplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/appimagemanager)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/org/kde/appimagemanager/
    DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/appimagemanager
    FILES_MATCHING PATTERN "qmldir" PATTERN "*.qmltypes"
)
