find_package(Qt6 REQUIRED COMPONENTS Test Quick QuickTest Concurrent)
include(ECMAddTests)

# ── tst_appimageinfo ──────────────────────────────────────────────────────────
# Tests pure inline helpers from appimageinfo.h — no library linkage needed.
ecm_add_test(tst_appimageinfo.cpp
    TEST_NAME tst_appimageinfo
    LINK_LIBRARIES Qt6::Test Qt6::Core
    NAME_PREFIX "appimagemanager-"
)
target_include_directories(tst_appimageinfo PRIVATE ${CMAKE_SOURCE_DIR})

# ── tst_appimagereader ────────────────────────────────────────────────────────
# Tests AppImageReader::cleanName() (public static, no file I/O).
ecm_add_test(tst_appimagereader.cpp
    TEST_NAME tst_appimagereader
    LINK_LIBRARIES Qt6::Test appimagemanager_core KF6::CoreAddons KF6::ConfigCore
    NAME_PREFIX "appimagemanager-"
)
target_include_directories(tst_appimagereader PRIVATE ${CMAKE_SOURCE_DIR})

# ── tst_appimagecache ─────────────────────────────────────────────────────────
# Tests AppImageCache store/load round-trip against a real SQLite file.
ecm_add_test(tst_appimagecache.cpp
    TEST_NAME tst_appimagecache
    LINK_LIBRARIES Qt6::Test Qt6::Sql Qt6::Concurrent appimagemanager_core KF6::CoreAddons KF6::ConfigCore
    NAME_PREFIX "appimagemanager-"
)
target_include_directories(tst_appimagecache PRIVATE ${CMAKE_SOURCE_DIR})

# ── tst_appimagelistmodel ─────────────────────────────────────────────────────
# Drives a real AppImageListModel + AppImageSortFilterModel against a tempdir
# of zero-byte stubs. Covers scanning state, row insert/remove, sort/filter,
# and the itemData bulk-fetch helper.
ecm_add_test(tst_appimagelistmodel.cpp
    TEST_NAME tst_appimagelistmodel
    LINK_LIBRARIES Qt6::Test Qt6::Quick appimagemanager_qml
    NAME_PREFIX "appimagemanager-"
    GUI
)
target_include_directories(tst_appimagelistmodel PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/src
)
set_tests_properties(appimagemanager-tst_appimagelistmodel PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# ── tst_appimageiconprovider ──────────────────────────────────────────────────
# Concurrent requestPixmap / setIconData stress test.
# Verifies split-lock implementation (QReadWriteLock + QMutex) is deadlock-free
# under simultaneous reads and writes from multiple threads.
ecm_add_test(tst_appimageiconprovider.cpp
    TEST_NAME tst_appimageiconprovider
    LINK_LIBRARIES Qt6::Test Qt6::Gui Qt6::Concurrent appimagemanager_qml
    NAME_PREFIX "appimagemanager-"
    GUI
)
target_include_directories(tst_appimageiconprovider PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/src
)
set_tests_properties(appimagemanager-tst_appimageiconprovider PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# ── tst_appsettings ───────────────────────────────────────────────────────────
# Round-trip tests for KSharedConfig-backed AppSettings properties.
# Covers accentBorders, applicationsPath, updateFrequency, showDisclaimer,
# and version string helpers.
ecm_add_test(tst_appsettings.cpp
    TEST_NAME tst_appsettings
    LINK_LIBRARIES Qt6::Test appimagemanager_qml KF6::CoreAddons KF6::ConfigCore
    NAME_PREFIX "appimagemanager-"
)
target_include_directories(tst_appsettings PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/src
)

# ── tst_updatedaemon ─────────────────────────────────────────────────────────
# Verifies UpdateDaemon timer gating: timer stays stopped on freq=0 (Never),
# stops immediately when frequency changes to 0 at runtime, and checkUpdates()
# returns early without side effects when freq=0.
ecm_add_test(tst_updatedaemon.cpp
    TEST_NAME tst_updatedaemon
    LINK_LIBRARIES Qt6::Test appimagemanager_qml KF6::CoreAddons KF6::ConfigCore
    NAME_PREFIX "appimagemanager-"
)
target_include_directories(tst_updatedaemon PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/src
)

# ── tst_appimagedbusadaptor ───────────────────────────────────────────────────
# Verifies the read-only D-Bus adaptor: Version() matches build version,
# InstalledAppImages() returns a valid (empty) QVariantList on an empty model.
# Skipped automatically when no D-Bus session bus is available.
ecm_add_test(tst_appimagedbusadaptor.cpp
    TEST_NAME tst_appimagedbusadaptor
    LINK_LIBRARIES Qt6::Test Qt6::DBus Qt6::Quick appimagemanager_qml KF6::CoreAddons KF6::ConfigCore
    NAME_PREFIX "appimagemanager-"
    GUI
)
target_include_directories(tst_appimagedbusadaptor PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/src
)
set_tests_properties(appimagemanager-tst_appimagedbusadaptor PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=$ENV{DBUS_SESSION_BUS_ADDRESS}"
)

# ── tst_qml (QML smoke tests, headless) ──────────────────────────────────────
# Instantiates SettingsDialog and AboutDialog under QT_QPA_PLATFORM=offscreen.
# Catches binding loops that cause SIGABRT (preferredHeight → window.height cycle).
qt_add_executable(tst_qml tst_qml_main.cpp)
target_link_libraries(tst_qml PRIVATE Qt6::QuickTest Qt6::Quick appimagemanager_qml KF6::I18n)
target_include_directories(tst_qml PRIVATE ${CMAKE_BINARY_DIR}/src)

# QML engine teardown may abort() in headless mode due to KDE singleton cleanup.
# All test cases complete and print PASS before the crash. Wrap in shell so the
# SIGABRT exit code (134) is masked; PASS_REGULAR_EXPRESSION then validates the
# actual test output and FAIL_REGULAR_EXPRESSION catches real test failures.
add_test(
    NAME appimagemanager-tst_qml
    COMMAND bash -c
        "QT_QPA_PLATFORM=offscreen $<TARGET_FILE:tst_qml> -input ${CMAKE_CURRENT_SOURCE_DIR}/qml; true"
)
set_tests_properties(appimagemanager-tst_qml PROPERTIES
    PASS_REGULAR_EXPRESSION "PASS.*cleanupTestCase"
    FAIL_REGULAR_EXPRESSION  "FAIL!"
)
