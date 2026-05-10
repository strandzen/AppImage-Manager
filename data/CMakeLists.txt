ecm_install_icons(
    ICONS sc-apps-appimagemanager.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor
)

install(FILES appimagemanager.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR}
)

install(FILES servicemenus/appimagemanager.desktop
    DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus
)

install(FILES io.github.strandzen.AppImageManager.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)

install(FILES appimagemanager.metainfo.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)
