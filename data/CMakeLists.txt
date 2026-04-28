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

install(FILES appimagemanager.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)

