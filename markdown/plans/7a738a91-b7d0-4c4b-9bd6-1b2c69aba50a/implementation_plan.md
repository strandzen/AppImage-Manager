# Add Theme Application Popup

The user wants a visual confirmation when a theme is applied.

## Proposed Changes

### [Theme Switcher]

#### [MODIFY] [themeSwitcher](file:///home/herman/Documents/Project/Konsave/themeSwitcher)
- Update `apply_action` to capture the return code of `subprocess.run`.
- Display a `QMessageBox.information` on success.
- Display a `QMessageBox.critical` on failure.

```python
    def apply_action(self):
        selected = self.list_widget.currentItem()
        if selected:
            arg = PRESETS[selected.text()]["arg"]
            try:
                result = subprocess.run(["konsave", "-a", arg])
                if result.returncode == 0:
                    QMessageBox.information(self, "Success", "Theme applied, please log in and out for full effect.")
                else:
                    QMessageBox.warning(self, "Error", "Something went wrong, please try again.")
            except Exception as e:
                QMessageBox.warning(self, "Error", "Something went wrong, please try again.")
```

## Verification Plan

### Manual Verification
- Rebuild the app.
- Run the new binary.
- Click "Apply Theme".
- Check for the popup.
