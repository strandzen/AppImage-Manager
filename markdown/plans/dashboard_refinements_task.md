# Task List - Dashboard Layout Refinements and About Page Restyling

- [x] Add `sortAscending` property and alphabetic sorting logic to C++ `AMStoreModel`
- [x] Modify `qml/StorePage.qml` to:
  - [x] Remove category chips from list entries
  - [x] Remove the "All" category button and implement toggle behavior on category filter chips
  - [x] Add A-Z sorting button next to the search field
- [x] Modify `qml/DashboardWindow.qml` to:
  - [x] Anchors "Settings" and "About" to the bottom of the navigation pane
  - [x] Add collapse/expand property and a top bar toggle action
  - [x] Make navigation list items match native rounded highlights and support compact icons-only display
- [x] Rewrite `qml/AboutPage.qml` to use the `FormCard` style from KirigamiAddons (unified with `SettingsPage.qml`)
- [x] Compile and verify execution
- [x] Verify unit tests pass
