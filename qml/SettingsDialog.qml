// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import appimagemanager

Kirigami.Dialog {
    id: dialog
    title: i18n("Settings")
    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton

    Kirigami.FormLayout {
        Controls.TextField {
            Kirigami.FormData.label: i18n("Applications folder:")
            text: AppSettings.applicationsPath
            onEditingFinished: AppSettings.applicationsPath = text
        }
        Controls.CheckBox {
            text: i18n("Show security disclaimer")
            checked: AppSettings.showDisclaimer
            onToggled: AppSettings.showDisclaimer = checked
        }
    }
}
