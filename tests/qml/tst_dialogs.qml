// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick
import QtTest
import appimagemanager

// Smoke tests for dialog instantiation.
// Verifies that SettingsDialog and AboutDialog can be created without
// triggering binding loops (which cause SIGABRT via free(): invalid size).
//
// Run headlessly: QT_QPA_PLATFORM=offscreen ctest -R tst_qml
TestCase {
    id: root
    name: "DialogSmokeTests"
    when: windowShown

    // ── SettingsDialog ────────────────────────────────────────────────────────

    Component {
        id: settingsComp
        SettingsDialog {}
    }

    function test_settingsDialog_instantiates() {
        // If preferredHeight binds to window.height, this triggers a binding
        // loop → SIGABRT. Passing means the fixed constant value is in place.
        var dlg = createTemporaryObject(settingsComp, root)
        verify(dlg !== null, "SettingsDialog failed to instantiate")
        verify(dlg.preferredHeight > 0, "preferredHeight must be positive")
        // Must NOT be bound to window geometry — fixed grid-unit value expected.
        verify(dlg.preferredHeight <= 800, "preferredHeight suspiciously large — possible binding loop")
        dlg.destroy()
    }

    function test_settingsDialog_opens_without_crash() {
        var dlg = createTemporaryObject(settingsComp, root)
        verify(dlg !== null)
        dlg.open()
        // Allow one frame for binding evaluation
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
