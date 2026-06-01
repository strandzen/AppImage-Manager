# Simple Liquid Glass Meditation App

Build a simple, beautiful SwiftUI iOS meditation application with a "Liquid Glass" aesthetic that enables users to quickly start a meditation session without friction. The app features a time picker, interval sounds, background sounds, a relaxing darkened active-meditation screen, and sets the foundation for integrating Apple HealthKit and mood tracking.

## User Review Required

> [!IMPORTANT]
> You are currently using a Linux environment, but iOS development (SwiftUI with full native functionality and HealthKit) requires macOS and Xcode to compile and build the final `.xcodeproj`. 
> 
> I will structure the project locally by creating a new directory `~/Documents/Project/LiquidMeditation` and provide all the `.swift` files, an `assets` directory guide, and the Xcode project structure instructions. You will then need to clone or copy this to a macOS environment to run it. Is this approach perfectly fine with you?

## Proposed Changes

We will create a structured directory for your SwiftUI files.

---

### Core Architecture & State Management

#### [NEW] `LiquidMeditationApp.swift`
The main entry point of the app, configuring the global environment and passing down models.

#### [NEW] `MeditationViewModel.swift`
The central brain for tracking state:
- Manages the timer countdown (e.g., changing states from `setup` to `meditating`).
- Creates `AVAudioPlayer` instances for both background loops and interval pings.
- Adjusts phone screen brightness (using `UIScreen.main.brightness` when active, if desired) or just handles full-screen dark overlays.

#### [NEW] `SoundManager.swift`
Handles resolving sound names to bundle URLs, crossfading, ducking audio, and standardizing the audio session for background playback.

---

### UI Components (Liquid Glass Design)

#### [NEW] `HomeView.swift`
The main view immediately shown upon opening the app.
- Incorporates Liquid Glass styling: `.thinMaterial` modifier over fluid, slow-moving colored gradients.
- Uses a `Picker` with `.pickerStyle(WheelPickerStyle())` for the 1-90 minute time wheel.
- Provides sleek, glass-morphic toggle buttons and pickers for "Background Sound" and "Interval Bell".
- Contains the prominent glass "Meditate Now" button.
- Contains small corner buttons navigating to `SettingsView` and `CalendarView`.

#### [NEW] `ActiveMeditationView.swift`
A minimalist, dark-mode screen appearing when a session begins.
- Transitions to a dark, calming color palette.
- Displays the countdown gracefully.
- Animates subtly (e.g., slow breathing blobs or rings) so the user can look at the screen if they prefer open-eye meditation.

#### [NEW] `LiquidGlassStyles.swift`
A collection of custom `ViewModifier` and shapes to easily apply glassmorphism (translucency, subtle white borders, drop shadows) to buttons, containers, and backgrounds consistently.

#### [NEW] `MoodCheckInView.swift` (Stretch Goal)
A small modal or overlay view prompting the user to rate their mood from 1-4. This can be intercepted before the `ActiveMeditationView` launches and/or right after the timer completes.

---

## Open Questions

> [!NOTE]
> **To ensure I build exactly what you want, please clarify the following:**
> 1. **Time Wheel:** Do you prefer the standard iOS alarm clock scroller (digital wheel), or a custom circular drag wheel (like a steering wheel)?
> 2. **Intervals:** For the bell/gong/flute interval, should the user specify a duration (e.g., "play gong every 5 minutes") or just select "gong" and it plays at dynamic points?
> 3. **Sound Assets:** I don't have access to audio files. I will use placeholder filenames (e.g., `flute.mp3`) in the code. You will need to drop actual `.mp3` files with those names into your Xcode Assets later. Does that sound good?
> 4. **Mood Tracking:** Should the 1-4 scale be represented by emoticons (e.g., 😢 😐 🙂 😁) or just numbers?

## Verification Plan

### Manual Verification
- You will need to move the generated Swift codebase to a macOS machine with Xcode.
- Open the project, add the necessary `.mp3` files to the App Bundle.
- Run the app in the iOS Simulator or on a physical iPhone to ensure accurate timing, audio session handling (playing even if the screen turns off), and to experience the visual SwiftUI liquid glass aesthetics.
