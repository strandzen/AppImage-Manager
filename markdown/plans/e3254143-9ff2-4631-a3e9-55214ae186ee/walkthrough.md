# Full Update Automation

We have successfully integrated full delta updates and a background daemon into the AppImage Manager! Here is a summary of the new additions:

## 1. Zsync Delta Downloads
The application now supports downloading AppImage updates using `zsync`, a file transfer program that downloads only the changed parts of the file.
- When an update is detected, clicking the green **"Available"** chip now initiates the download.
- The `AppImageListModel` intercepts the `zsync` command's standard output to parse the current percentage.
- The UI dynamically swaps out the "Available" chip for a `Kirigami.ProgressBar`, showing the download progress directly in your Dashboard.
- Once the download is complete, the old AppImage is automatically replaced with the new one, and its metadata is refreshed seamlessly.
- You will receive a native Plasma notification upon successful completion.

## 2. Background Update Daemon
We added a `--daemon` flag to the main executable, allowing the app to run completely headless in the background.
- A new `UpdateDaemon` class runs silently without loading any UI components, ensuring a minimal memory footprint.
- A `org.kde.appimagemanager.daemon.desktop` autostart file is now installed to `/etc/xdg/autostart`, which launches the daemon automatically when you log into your Plasma session.
- If the daemon detects an update, it sends a persistent `KNotification` to the Plasma desktop. Clicking "Open Manager" on the notification opens the Dashboard where you can apply the update.

## 3. Configuration & Settings
You can customize how the daemon behaves from the Dashboard UI.
- In the **Settings Dialog**, there is a new "Background Updates" section.
- You can configure the update checking frequency using a dropdown (Never, Daily, Weekly, Monthly, or Custom).
- If "Custom" is selected, a new SpinBox appears to let you pick the exact number of days between checks.
- These settings are saved to `~/.config/appimagemanagerrc`.

> [!TIP]
> To test the daemon manually, you can run `./build_new/bin/appimagemanager --daemon` from your terminal. If you have any outdated AppImages, a notification should appear shortly!
