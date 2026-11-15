// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtTest
import appimagemanager

// Smoke tests for dialog instantiation.
// Verifies that UpdateSourceDialog, EnvironmentDialog and AboutDialog can be created without
// triggering binding loops (which cause SIGABRT via free(): invalid size).
//
// Run headlessly: QT_QPA_PLATFORM=offscreen ctest -R tst_qml
TestCase {
    id: root
    name: "DialogSmokeTests"
    when: windowShown

    // ── UpdateSourceDialog ────────────────────────────────────────────────────

    Component {
        id: updateSourceComp
        UpdateSourceDialog {}
    }

    function test_updateSourceDialog_opens_without_crash() {
        // openFor() needs the dashboard's proxyModel; plain open() checks the layout.
        var dlg = createTemporaryObject(updateSourceComp, root)
        verify(dlg !== null, "UpdateSourceDialog failed to instantiate")
        dlg.open()
        wait(50)
        dlg.close()
        wait(50)
        dlg.destroy()
    }

    // ── EnvironmentDialog ─────────────────────────────────────────────────────

    Component {
        id: environmentComp
        EnvironmentDialog {}
    }

    function test_environmentDialog_opens_without_crash() {
        var dlg = createTemporaryObject(environmentComp, root)
        verify(dlg !== null, "EnvironmentDialog failed to instantiate")
        dlg.open()
        wait(50)
        dlg.close()
        wait(50)
        dlg.destroy()
    }

    // ── AboutDialog ───────────────────────────────────────────────────────────

    Component {
        id: aboutComp
        AboutDialog {}
    }

    function test_aboutDialog_instantiates() {
        var dlg = createTemporaryObject(aboutComp, root)
        verify(dlg !== null, "AboutDialog failed to instantiate")
        verify(dlg.preferredHeight > 0)
        verify(dlg.preferredHeight <= 800, "AboutDialog preferredHeight suspiciously large")
        dlg.destroy()
    }

    function test_aboutDialog_opens_without_crash() {
        var dlg = createTemporaryObject(aboutComp, root)
        verify(dlg !== null)
        dlg.open()
        wait(50)
        dlg.close()
        wait(50)
        dlg.destroy()
    }
}
