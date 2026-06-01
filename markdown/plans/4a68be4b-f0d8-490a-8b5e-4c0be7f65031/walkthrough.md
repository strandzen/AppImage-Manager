# Walkthrough - KDE Qt Installer App
**GitHub Repo**: [strandzen/installer](https://github.com/strandzen/installer.git)


I have implemented the KDE-style Installer Application using Python and PyQt6.

## Prerequisites

Ensure you have `python3` and `pip` installed.
The application will automatically attempt to install `PyQt6` if it is missing.

```bash
# Example for Debian/Ubuntu (if pip is missing)
sudo apt install python3-pip
```

## How to Run

### Option 1: Standalone Binary (Recommended)
You can run the generated standalone binary directly without installing any dependencies:
```bash
./dist/installer-widget
```

### Option 2: Python Script
Navigate to the project directory and run `main.py`:

```bash
cd /home/herman/Documents/Project/Installer
python3 main.py
```

## Verification Steps

1. **Password Prompt**:
   - The app should launch and immediately ask for your `sudo` password.
   - **Test**: Enter a wrong password first. It should show an error and ask again.
   - **Test**: Enter the correct password. It should proceed.

2. **Confirmation List**:
   - You should see a window listing actions to be performed (System Updates, Dependencies, Flatpaks, etc.).
   - Verify the list makes sense based on your system state.

3. **Feature Verification**:
   - **Selection**: Uncheck some items in the list. Click **Confirm Installation**. Verify only checked items are executed in the terminal.
   - **Add Flatpak**: Enter a Flatpak ID (e.g., `org.kde.kcalc`) in the text box and click **Add Flatpak**. Verify it appears in the list as checked.
   - **Theme**: If running on KDE Plasma, the app should now match your system theme (e.g., Breeze Dark).

4. **Installation Process**:
   - Click **Confirm Installation**.
   - A black "Terminal" window should appear.
   - Watch the output. Commands should execute.
   - If updates are found or packages installed, you will see the output (e.g., `apt` or `flatpak` progress).

5. **Completion**:
   - When finished, a "Close" button should appear at the bottom of the Terminal window.

## Files Created

- [main.py](file:///home/herman/Documents/Project/Installer/main.py): Entry point.
- [logic.py](file:///home/herman/Documents/Project/Installer/logic.py): System scanning and command generation.
- [gui.py](file:///home/herman/Documents/Project/Installer/gui.py): UI implementation (Password Dialog, Main Window, Terminal).
