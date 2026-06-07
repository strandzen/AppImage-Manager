# First-Time User Onboarding Wizard

Implement a step-by-step onboarding experience for first-time users to introduce AppImage Manager, explain how the app works, and allow them to configure their Applications folder.

## User Review Required

> [!NOTE]
> We will add a new `firstRun` boolean setting to KConfig settings (with a default value of `true`). On the first run, the dashboard will open an onboarding wizard modal dialog. Once the onboarding is completed (or skipped), `firstRun` is set to `false`.

## Open Questions

None. The design constraints and functional requirements are fully defined.

## Proposed Changes

### Configuration & Settings (Core)

We will introduce a new setting `firstRun` in KConfig and expose it to QML via `AppSettings`.

---

#### [MODIFY] [appimagemanagersettings.kcfg](file:///home/herman/Documents/Project/AppImageManager/src/core/appimagemanagersettings.kcfg)

Add a `firstRun` boolean entry under the `General` group.

```xml
    <entry name="firstRun" type="Bool">
      <default>true</default>
    </entry>
```

#### [MODIFY] [appsettings.h](file:///home/herman/Documents/Project/AppImageManager/src/core/appsettings.h)

Declare a new Q_PROPERTY for `firstRun`, alongside its getter, setter, and notification signal.

```cpp
    Q_PROPERTY(bool    firstRun          READ firstRun          WRITE setFirstRun          NOTIFY firstRunChanged)
...
    bool firstRun() const;
    void setFirstRun(bool enabled);
...
Q_SIGNALS:
    void firstRunChanged();
```

#### [MODIFY] [appsettings.cpp](file:///home/herman/Documents/Project/AppImageManager/src/core/appsettings.cpp)

Implement the getter, setter, and signal triggers for `firstRun` in `AppSettings`.

```cpp
bool AppSettings::firstRun() const
{
    return AppImageManagerSettings::self()->firstRun();
}

void AppSettings::setFirstRun(bool enabled)
{
    if (firstRun() == enabled)
        return;
    AppImageManagerSettings::self()->setFirstRun(enabled);
    AppImageManagerSettings::self()->save();
    Q_EMIT firstRunChanged();
}
```

Add `Q_EMIT firstRunChanged();` to `resetToDefaults()`.

---

### Build System & QML Pages (GUI)

Register and construct the onboarding dialog and trigger it on dashboard initialization.

---

#### [MODIFY] [CMakeLists.txt](file:///home/herman/Documents/Project/AppImageManager/src/CMakeLists.txt)

Add `qml/OnboardingDialog.qml` to the list of QML files and properties.

```cmake
    ${CMAKE_SOURCE_DIR}/qml/OnboardingDialog.qml
```

#### [NEW] [OnboardingDialog.qml](file:///home/herman/Documents/Project/AppImageManager/qml/OnboardingDialog.qml)

Create a custom wizard dialog that contains:
- **Slide 1: Welcome**: Introduces the application.
- **Slide 2: Applications Folder**: Explains how AppImages are stored and integrated. Provides a file selection button to customize the Applications folder path (`AppSettings.applicationsPath`).
- **Slide 3: Integration Overview**: Explains context menu integration (Dolphin) and download folder monitoring. Includes a checkbox to toggle download monitoring.
- **Slide 4: Ready**: Encourages user to get started.
- **Navigation Controls**: Next/Back buttons and a "Skip Onboarding" button.

#### [MODIFY] [DashboardWindow.qml](file:///home/herman/Documents/Project/AppImageManager/qml/DashboardWindow.qml)

Instantiate `OnboardingDialog` and display it automatically on startup if `AppSettings.firstRun` is true.

```qml
    OnboardingDialog { id: onboardingDialog }
    
    Component.onCompleted: {
        if (AppSettings.firstRun) {
            onboardingDialog.open()
        }
    }
```

---

## Verification Plan

### Automated Tests
- Run the build and make sure it compiles cleanly:
  ```bash
  cmake --preset dev
  cmake --build --preset dev
  ```
- Run unit tests to verify no regressions:
  ```bash
  ctest --test-dir build/dev --output-on-failure
  ```

### Manual Verification
- Install and launch the dashboard.
- Verify that the Onboarding dialog displays on the first run.
- Verify that the Applications folder can be updated in the onboarding flow, and changes reflect in the main Settings page.
- Complete the onboarding and close the app.
- Reopen the app and verify the Onboarding dialog does not reappear.
- Go to Settings, hit "Restore Defaults", verify that Onboarding is reset and appears again on next app restart or immediately.
