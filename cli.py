import argparse
import sys
import os
import core
import logging

def setup_service_menu():
    """Installs the KDE Plasma 6 Service Menu and Application Handler"""
    # 1. Plasma 6 service menu directory (for right-click)
    service_menu_dir = os.path.expanduser('~/.local/share/kio/servicemenus')
    os.makedirs(service_menu_dir, exist_ok=True)
    
    desktop_file_path = os.path.join(service_menu_dir, 'appimagemanager.desktop')
    
    # Path to this cli.py script
    script_path = os.path.abspath(__file__)
    
    # Ensure cli.py is executable
    os.chmod(script_path, 0o755)
    
    content = f"""[Desktop Entry]
Type=Service
MimeType=application/vnd.appimage;application/x-executable;
Actions=manageAppImage;

[Desktop Action manageAppImage]
Name=Manage AppImage
Icon=application-x-executable
Exec=python3 "{script_path}" gui "%f"
"""
    with open(desktop_file_path, 'w') as f:
        f.write(content)
    os.chmod(desktop_file_path, 0o755)
        
    logging.info(f"Service Menu installed to {desktop_file_path}")
    logging.info("You can now right-click any .AppImage file in Dolphin and select 'Manage AppImage'.")

def main():
    parser = argparse.ArgumentParser(description="AppImage Manager")
    subparsers = parser.add_subparsers(dest="command", help="Command to run")
    
    # Setup command
    subparsers.add_parser("setup", help="Install KDE Plasma Service Menu")
    
    # GUI command
    gui_parser = subparsers.add_parser("gui", help="Open GUI for an AppImage")
    gui_parser.add_argument("path", help="Path to the AppImage")
    
    # Install command
    install_parser = subparsers.add_parser("install", help="Install an AppImage to ~/Applications")
    install_parser.add_argument("path", help="Path to the AppImage")
    
    # Uninstall command
    uninstall_parser = subparsers.add_parser("uninstall", help="Find corpses for an AppImage")
    uninstall_parser.add_argument("path", help="Path to the AppImage")
    
    args = parser.parse_args()
    
    if args.command == "setup":
        setup_service_menu()
    elif args.command == "gui":
        # We will import gui module here so we don't require PySide6 if someone just runs CLI
        import gui
        gui.main(args.path)
    elif args.command == "install":
        new_path = core.install_appimage(args.path)
        logging.info(f"Installed to {new_path}")
    elif args.command == "uninstall":
        corpses = core.find_app_corpses(args.path)
        logging.info(f"Found {len(corpses)} corpses:")
        for c in corpses:
            logging.info(f"- {c['path']} ({c['size']} bytes)")
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
