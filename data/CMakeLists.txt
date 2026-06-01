ecm_install_icons(
    ICONS sc-apps-appimagemanager.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor
)

install(FILES appimagemanager.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR}
)

find_program(MSGFMT_EXECUTABLE msgfmt)
if(MSGFMT_EXECUTABLE)
    set(DESKTOP_IN "${CMAKE_CURRENT_SOURCE_DIR}/io.github.strandzen.AppImageManager.desktop")
    set(DESKTOP_OUT "${CMAKE_CURRENT_BINARY_DIR}/io.github.strandzen.AppImageManager.desktop")
    
    set(SERVICE_IN "${CMAKE_CURRENT_SOURCE_DIR}/servicemenus/appimagemanager.desktop")
    set(SERVICE_OUT "${CMAKE_CURRENT_BINARY_DIR}/servicemenus_appimagemanager.desktop")
    
    set(MERGE_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/merge_desktop.cmake")
    file(WRITE "${MERGE_SCRIPT}" "
file(GLOB PO_DIRS \"${CMAKE_SOURCE_DIR}/po/*\")
file(MAKE_DIRECTORY \"${CMAKE_CURRENT_BINARY_DIR}/po_flat\")
file(REMOVE \"${CMAKE_CURRENT_BINARY_DIR}/po_flat/LINGUAS\")
foreach(PO_DIR IN LISTS PO_DIRS)
    if(IS_DIRECTORY \"\${PO_DIR}\")
        get_filename_component(LANG \"\${PO_DIR}\" NAME)
        if(EXISTS \"\${PO_DIR}/appimagemanager.po\")
            file(COPY \"\${PO_DIR}/appimagemanager.po\" DESTINATION \"${CMAKE_CURRENT_BINARY_DIR}/po_flat\")
            file(RENAME \"${CMAKE_CURRENT_BINARY_DIR}/po_flat/appimagemanager.po\" \"${CMAKE_CURRENT_BINARY_DIR}/po_flat/\${LANG}.po\")
            file(APPEND \"${CMAKE_CURRENT_BINARY_DIR}/po_flat/LINGUAS\" \"\${LANG}\\n\")
        endif()
    endif()
endforeach()
execute_process(
    COMMAND ${MSGFMT_EXECUTABLE} --desktop --template \"${DESKTOP_IN}\" -d \"${CMAKE_CURRENT_BINARY_DIR}/po_flat\" -o \"${DESKTOP_OUT}\"
    RESULT_VARIABLE ret
)
if(NOT ret EQUAL 0)
    message(FATAL_ERROR \"msgfmt --desktop failed\")
endif()
execute_process(
    COMMAND ${MSGFMT_EXECUTABLE} --desktop --template \"${SERVICE_IN}\" -d \"${CMAKE_CURRENT_BINARY_DIR}/po_flat\" -o \"${SERVICE_OUT}\"
    RESULT_VARIABLE ret
)
if(NOT ret EQUAL 0)
    message(FATAL_ERROR \"msgfmt --desktop failed for service menu\")
endif()
")

    add_custom_command(
        OUTPUT "${DESKTOP_OUT}" "${SERVICE_OUT}"
        COMMAND ${CMAKE_COMMAND} -P "${MERGE_SCRIPT}"
        DEPENDS "${DESKTOP_IN}" "${SERVICE_IN}" "${CMAKE_SOURCE_DIR}/po"
        COMMENT "Merging translations into desktop files..."
    )
    add_custom_target(desktop_translations ALL DEPENDS "${DESKTOP_OUT}" "${SERVICE_OUT}")
    
    install(FILES "${DESKTOP_OUT}" DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES "${SERVICE_OUT}" DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus RENAME appimagemanager.desktop)
else()
    install(FILES io.github.strandzen.AppImageManager.desktop DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES servicemenus/appimagemanager.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus)
endif()

install(FILES appimagemanager.metainfo.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)

configure_file(
    io.github.appimagemanager.Daemon.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/io.github.appimagemanager.Daemon.service
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.appimagemanager.Daemon.service
    DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
)
