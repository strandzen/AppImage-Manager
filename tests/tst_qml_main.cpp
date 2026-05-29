// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include <KLocalizedQmlContext>

// Called by qmltestrunner before each test file's engine is used.
// Installs i18n() and friends so dialogs that call i18n() don't crash.
class Setup : public QObject
{
    Q_OBJECT
public:
    Setup() = default;

public Q_SLOTS:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        KLocalization::setupLocalizedContext(engine);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml, Setup)
#include "tst_qml_main.moc"
