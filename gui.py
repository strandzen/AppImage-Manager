import sys
import os
import tempfile
import logging
import threading
from pathlib import Path
from PySide6.QtCore import QObject, Slot, Property, Signal, QUrl, QSize, QTranslator, QLocale
from PySide6.QtGui import QGuiApplication, QIcon, QPixmap, QImage
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickImageProvider
from PySide6.QtCore import QByteArray
import core

class AppImageIconProvider(QQuickImageProvider):
    def __init__(self):
        super().__init__(QQuickImageProvider.Pixmap)
        self.icon_data = None

    def set_icon_data(self, data: bytes):
        self.icon_data = data

    def requestPixmap(self, id_str, size, requestedSize):
        pixmap = QPixmap()
        if self.icon_data:
            pixmap.loadFromData(QByteArray(self.icon_data))
        
        if requestedSize.isValid():
            return pixmap.scaled(requestedSize)
        return pixmap

class ThemeIconProvider(QQuickImageProvider):
    def __init__(self):
        super().__init__(QQuickImageProvider.Pixmap)
        
    def requestPixmap(self, id_str, size, requestedSize):
        icon = QIcon.fromTheme(id_str)
        if requestedSize.isValid():
            res_size = requestedSize
        else:
            res_size = QSize(128, 128) # fallback size
        return icon.pixmap(res_size)

class AppImageManagerBackend(QObject):
    infoChanged = Signal()
    corpsesChanged = Signal()
    installedChanged = Signal()
    desktopLinkChanged = Signal()
    uninstallFinished = Signal()

    def __init__(self, appimage_path, icon_provider):
        super().__init__()
        self._appimage_path = os.path.abspath(appimage_path)
        self._info = core.get_appimage_info(self._appimage_path)
        self._corpses = []
        
        # Determine if it's already in ~/Applications
        applications_dir = os.path.expanduser('~/Applications')
        self._is_installed = self._appimage_path.startswith(applications_dir)
        
        self._has_desktop_link = core.is_desktop_link_enabled(self._appimage_path)
        
        if self._info.get('icon_data'):
            icon_provider.set_icon_data(self._info['icon_data'])

    @Property(str, notify=infoChanged)
    def appName(self):
        return self._info.get('name', 'Unknown')

    @Property(str, notify=infoChanged)
    def cleanName(self):
        return self._info.get('clean_name', 'Unknown')

    @Property(str, notify=infoChanged)
    def originalName(self):
        return self._info.get('original_name', 'Unknown')

    @Property(str, notify=infoChanged)
    def appVersion(self):
        return self._info.get('version', 'Unknown')

    @Property(int, notify=infoChanged)
    def appSize(self):
        return self._info.get('size', 0)

    @Property(str, notify=infoChanged)
    def appIconSource(self):
        if self._info.get('icon_data'):
            return "image://appimage/icon"
        return ""

    @Property(bool, notify=installedChanged)
    def isInstalled(self):
        return self._is_installed

    @Property(bool, notify=desktopLinkChanged)
    def hasDesktopLink(self):
        return self._has_desktop_link

    @Property(list, notify=corpsesChanged)
    def corpses(self):
        return self._corpses

    @Slot()
    def installAppImage(self):
        def task():
            try:
                new_path = core.install_appimage(self._appimage_path)
                self._appimage_path = new_path
                self._is_installed = True
                
                # Re-check desktop link status
                self._has_desktop_link = core.is_desktop_link_enabled(self._appimage_path)
                
                self.installedChanged.emit()
                self.desktopLinkChanged.emit()
            except Exception as e:
                logging.error(f"Error installing: {e}")
                
        threading.Thread(target=task, daemon=True).start()

    @Slot(bool)
    def toggleDesktopLink(self, enable):
        try:
            core.manage_desktop_link(self._appimage_path, enable)
            self._has_desktop_link = enable
            self.desktopLinkChanged.emit()
        except Exception as e:
            logging.error(f"Error toggling desktop link: {e}")

    @Slot()
    def findCorpses(self):
        self._corpses = core.find_app_corpses(self._appimage_path)
        self.corpsesChanged.emit()

    @Slot(list)
    def removeAppImageAndCorpses(self, paths_to_delete):
        try:
            # Paths to delete from QML comes as a list of strings
            core.remove_items(self._appimage_path, paths_to_delete)
            self.uninstallFinished.emit()
        except Exception as e:
            logging.error(f"Error removing items: {e}")

    def cleanup(self):
        pass

def main(appimage_path):
    # Enable High DPI scaling
    os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "1"
    os.environ["QT_AUTO_SCREEN_SCALE_FACTOR"] = "1"
    
    app = QGuiApplication(sys.argv)
    
    # Try to set an app icon
    app.setWindowIcon(QIcon.fromTheme("application-x-executable"))
    
    # Setup i18n
    translator = QTranslator()
    locale = QLocale.system().name()
    if translator.load(f"appimagemanager_{locale}", os.path.dirname(__file__)):
        app.installTranslator(translator)
    
    engine = QQmlApplicationEngine()
    engine.addImageProvider("theme", ThemeIconProvider())
    
    app_icon_provider = AppImageIconProvider()
    engine.addImageProvider("appimage", app_icon_provider)
    
    backend = AppImageManagerBackend(appimage_path, app_icon_provider)
    engine.rootContext().setContextProperty("backend", backend)
    
    qml_file = os.path.join(os.path.dirname(__file__), "qml", "ManageWindow.qml")
    engine.load(QUrl.fromLocalFile(qml_file))
    
    if not engine.rootObjects():
        sys.exit(-1)
        
    res = app.exec()
    backend.cleanup()
    sys.exit(res)
