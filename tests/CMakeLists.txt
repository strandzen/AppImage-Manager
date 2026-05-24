find_package(Qt6 REQUIRED COMPONENTS Test Quick)
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
    LINK_LIBRARIES Qt6::Test Qt6::Sql appimagemanager_core KF6::CoreAddons KF6::ConfigCore
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
