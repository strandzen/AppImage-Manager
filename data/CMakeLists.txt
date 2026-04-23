ecm_install_icons(
    ICONS sc-apps-org.kde.appimagemanager.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor
)

install(FILES appimagemanager.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR}
)

install(FILES servicemenus/org.kde.appimagemanager.desktop
    DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus
)

