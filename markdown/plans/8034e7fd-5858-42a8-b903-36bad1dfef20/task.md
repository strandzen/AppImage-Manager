# Liquid Meditation App Tasks

- `[x]` **Project Setup**
  - `[x]` Create directory structure (`Views`, `Models`, `ViewModels`, `Styles`)
- `[x]` **Models & State Management**
  - `[x]` `AudioSettings.swift` (Interval options: off, bells, flutes, gongs; frequencies: start & end, 1m, 2m, 5m, 10m. Backgrounds: off, rain, waves, white noise).
  - `[x]` `SoundManager.swift` (AVAudioPlayer setup for intervals and backgrounds).
  - `[x]` `MeditationViewModel.swift` (Timer logic, sound triggering).
- `[x]` **UI & Styling**
  - `[x]` `LiquidGlassStyles.swift` (Glassmorphism modifiers, color palettes).
  - `[x]` `MinimalWatchTimePicker.swift` (Custom view for time wheel, minimal watch lines).
- `[x]` **Core Views**
  - `[x]` `HomeView.swift` (Main screen, UI assembly).
  - `[x]` `ActiveMeditationView.swift` (Darkened state with countdown).
  - `[x]` `LiquidMeditationApp.swift` (App entry point).
- `[x]` **Stretch Goals / Overlays**
  - `[x]` `MoodCheckInView.swift` (1-4 scale survey with numbers).
  - `[x]` `CalendarView.swift` (Placeholder for HealthKit/History).
  - `[x]` `SettingsView.swift` (Placeholder).
