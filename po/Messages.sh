#! /usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#
# Regenerate the POT template from source files.
# Run from the repository root:
#   XGETTEXT=xgettext podir=po bash po/Messages.sh

$XGETTEXT \
    src/main.cpp \
    src/core/appimagemanager.cpp \
    src/core/updatedaemon.cpp \
    src/core/updatesource.cpp \
    src/core/checkresults.cpp \
    src/core/releasechecker.cpp \
    src/gui/appimagebackend.cpp \
    src/gui/appimagelistmodel.cpp \
    src/gui/appimageupdate.cpp \
    src/gui/amstoremodel.cpp \
    src/gui/dashboardwindow.cpp \
    src/gui/traymodel.cpp \
    --keyword=i18n:1 \
    --keyword=i18nc:1c,2 \
    --keyword=i18np:1,2 \
    --keyword=i18ncp:1c,2,3 \
    --language=C++ \
    --from-code=UTF-8 \
    --add-comments=TRANSLATORS: \
    -o $podir/appimagemanager.pot

$XGETTEXT \
    qml/ManageWindow.qml \
    qml/UninstallDialog.qml \
    qml/DashboardWindow.qml \
    qml/LibraryPage.qml \
    qml/StorePage.qml \
    qml/SettingsPage.qml \
    qml/OnboardingDialog.qml \
    qml/UpdateSourceDialog.qml \
    qml/EnvironmentDialog.qml \
    qml/AboutDialog.qml \
    qml/AboutPage.qml \
    qml/AboutContent.qml \
    qml/ContributorsModel.qml \
    qml/TrayWindow.qml \
    qml/UpdateCheckToast.qml \
    --keyword=i18n:1 \
    --keyword=i18nc:1c,2 \
    --keyword=i18np:1,2 \
    --keyword=i18ncp:1c,2,3 \
    --language=JavaScript \
    --from-code=UTF-8 \
    --add-comments=TRANSLATORS: \
    --join-existing \
    -o $podir/appimagemanager.pot

# The tray widget runs in plasmashell and uses this catalog explicitly (i18nd).
$XGETTEXT \
    data/plasmoid/contents/ui/main.qml \
    --keyword=i18nd:2 \
    --keyword=i18ndc:2c,3 \
    --keyword=i18ndp:2,3 \
    --language=JavaScript \
    --from-code=UTF-8 \
    --join-existing \
    -o $podir/appimagemanager.pot
