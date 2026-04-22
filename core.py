import os
import shutil
import subprocess
import glob
import re
import tempfile
import configparser
import logging
import shlex
from functools import lru_cache
from pathlib import Path

def setup_logging():
    log_dir = os.path.expanduser('~/.local/state/appimagemanager')
    os.makedirs(log_dir, exist_ok=True)
    log_file = os.path.join(log_dir, 'appimagemanager.log')
    logging.basicConfig(
        filename=log_file,
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s'
    )

setup_logging()

def _clean_name(filename):
    """
    Cleans up an AppImage filename by removing architecture, typical suffixes, and replacing hyphens with spaces.
    Example: Bitwarden-2023.1.0-x86_64.AppImage -> Bitwarden 2023.1.0.AppImage
    """
    base = os.path.basename(filename)
    if not base.lower().endswith('.appimage'):
        return base
    
    name_without_ext = base[:-9]
    # Remove architecture tags
    for arch in ['-x86_64', '_x86_64', '-aarch64', '-arm64', '-amd64', '_amd64']:
        if name_without_ext.endswith(arch):
            name_without_ext = name_without_ext[:-len(arch)]
            
    # Replace hyphens and underscores with spaces
    cleaned = name_without_ext.replace('-', ' ').replace('_', ' ')
    
    # Capitalize nicely
    cleaned = ' '.join(word.capitalize() if not word.isupper() else word for word in cleaned.split())
    return f"{cleaned}.AppImage"

def _get_app_id(filename):
    """
    Extracts a clean application ID for theme icon lookups.
    Example: krita-5.3.1-x86_64.AppImage -> krita
    """
    name = filename.lower().replace('.appimage', '')
    match = re.match(r'^([a-z0-9]+)', name)
    if match:
        return match.group(1)
    return name.split('-')[0].split('_')[0].split('.')[0]

@lru_cache(maxsize=10)
def get_appimage_info(path):
    """
    Extracts metadata from an AppImage.
    Returns a dict with: name, version, size, original_name, clean_name, icon_path, exec_args
    """
    if not os.path.isfile(path):
        raise FileNotFoundError(f"AppImage not found: {path}")

    size = os.path.getsize(path)
    original_name = os.path.basename(path)
    clean_name = _clean_name(original_name)
    
    info = {
        'name': original_name.replace('.AppImage', ''),
        'version': 'Unknown',
        'size': size,
        'original_name': original_name,
        'clean_name': clean_name,
        'app_id': _get_app_id(original_name),
        'icon_data': None, # We'll extract this later if needed, or pass the path
        'icon_ext': '',
        'exec_args': '',
        'categories': ''
    }

    # Create a temporary directory to extract into
    with tempfile.TemporaryDirectory() as temp_dir:
        # We try to extract only the .desktop file and the .png/.svg icon
        # Not all AppImages support extracting specific files easily, so we might have to extract all
        # or use squashfuse if available, but for now we extract all into temp_dir.
        # This can be slow for large AppImages, but it's the most reliable way without FUSE.
        try:
            squashfs_root = os.path.join(temp_dir, 'squashfs-root')
            mounted = False
            
            # Try squashfuse first for instant mounting
            try:
                os.makedirs(squashfs_root, exist_ok=True)
                cmd = ['squashfuse', path, squashfs_root]
                proc = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                mounted = (proc.returncode == 0)
            except FileNotFoundError:
                pass # squashfuse not installed
                
            if not mounted:
                # Fallback to extraction
                cmd = [path, '--appimage-extract']
                process = subprocess.run(cmd, cwd=temp_dir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if process.returncode != 0:
                    return info # Extraction failed completely
            
            try:
                desktop_files = glob.glob(os.path.join(squashfs_root, '*.desktop'))
                if desktop_files:
                    desktop_file = desktop_files[0]
                    config = configparser.ConfigParser()
                    config.read(desktop_file, encoding='utf-8')
                    
                    if 'Desktop Entry' in config:
                        entry = config['Desktop Entry']
                        info['name'] = entry.get('Name', info['name'])
                        info['version'] = entry.get('X-AppImage-Version', 'Unknown')
                        info['categories'] = entry.get('Categories', '')
                        
                        exec_full = entry.get('Exec', '')
                        exec_parts = exec_full.split()
                        if len(exec_parts) > 1:
                            # Use shlex to safely handle and quote arguments, omitting desktop spec macros
                            args = [p for p in exec_parts[1:] if not p.startswith('%')]
                            sanitized_args = []
                            for arg in args:
                                sanitized_args.append(shlex.quote(arg))
                            
                            info['exec_args'] = ' '.join(sanitized_args)
                        
                        # Improved Icon Finding
                        icon_name = entry.get('Icon', '')
                        candidate_paths = []
                        
                        if icon_name:
                            candidate_paths.append((os.path.join(squashfs_root, f"{icon_name}.png"), '.png'))
                            candidate_paths.append((os.path.join(squashfs_root, f"{icon_name}.svg"), '.svg'))
                            candidate_paths.append((os.path.join(squashfs_root, "usr/share/icons/hicolor/256x256/apps", f"{icon_name}.png"), '.png'))
                            candidate_paths.append((os.path.join(squashfs_root, "usr/share/icons/hicolor/scalable/apps", f"{icon_name}.svg"), '.svg'))
                        
                        candidate_paths.append((os.path.join(squashfs_root, ".DirIcon"), '.png'))
                        
                        if not icon_name:
                            for f in os.listdir(squashfs_root):
                                if f.lower().endswith(('.png', '.svg')):
                                    candidate_paths.append((os.path.join(squashfs_root, f), f[-4:]))

                        found_icon = None
                        for icon_path, ext in candidate_paths:
                            if os.path.exists(icon_path):
                                found_icon = icon_path
                                info['icon_ext'] = ext
                                break
                            
                        if found_icon:
                            try:
                                with open(found_icon, 'rb') as f:
                                    info['icon_data'] = f.read()
                            except Exception as e:
                                logging.error(f"Failed to read icon file {found_icon}: {e}")
            finally:
                if mounted:
                    subprocess.run(['fusermount', '-u', squashfs_root], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        except Exception as e:
            logging.error(f"Failed to extract info from AppImage: {e}")

    return info

def install_appimage(path):
    """
    Moves the AppImage to ~/Applications and makes it executable.
    Returns the new path.
    """
    applications_dir = os.path.expanduser('~/Applications')
    os.makedirs(applications_dir, exist_ok=True)
    
    original_name = os.path.basename(path)
    new_path = os.path.join(applications_dir, original_name)
    
    if path != new_path:
        # Use KIO for native file movement and progress notifications
        try:
            subprocess.run(['kioclient', 'move', path, new_path], check=True)
        except Exception as e:
            logging.error(f"KIO move failed, falling back to shutil: {e}")
            shutil.move(path, new_path)
        
    # Make executable
    os.chmod(new_path, 0o755)
    return new_path

def manage_desktop_link(appimage_path, enable):
    """
    Creates or removes a .desktop file in ~/.local/share/applications/
    and manages the associated icon in ~/.local/share/icons/AppImages/
    """
    desktop_dir = os.path.expanduser('~/.local/share/applications')
    icons_dir = os.path.expanduser('~/.local/share/icons/AppImages')
    
    info = get_appimage_info(appimage_path)
    clean_name = info['clean_name'].replace('.AppImage', '')
    desktop_file = os.path.join(desktop_dir, f"appimage_{clean_name.replace(' ', '_').lower()}.desktop")
    icon_path = os.path.join(icons_dir, f"appimage_{clean_name.replace(' ', '_').lower()}{info.get('icon_ext', '.png')}")
    
    if enable:
        os.makedirs(desktop_dir, exist_ok=True)
        os.makedirs(icons_dir, exist_ok=True)
        
        # Save icon
        app_id = info.get('app_id', clean_name.replace(' ', '_').lower())
        if info.get('icon_data'):
            with open(icon_path, 'wb') as f:
                f.write(info['icon_data'])
            final_icon = icon_path # Fallback to local file if theme icon fails
        else:
            final_icon = app_id # Use theme icon directly
            
        # Prioritize system theme by setting Icon=app_id. KDE will use the theme icon if it exists,
        # otherwise it will fall back to the extracted icon if it's named app_id in the system, 
        # or we just write final_icon which is either the absolute path to the extracted icon or the app_id.
        # Actually, using the absolute path to the extracted icon is safest, but using app_id lets themes override it.
        # Let's set Icon=app_id. If KDE can't find it, we'll try absolute path.
        desktop_icon = app_id if app_id else final_icon

        # Create .desktop file
        exec_str = f'"{appimage_path}"'
        if info.get('exec_args'):
            exec_str += f" {info['exec_args']}"
            
        desktop_content = f"""[Desktop Entry]
Name={clean_name}
Exec={exec_str}
Icon={desktop_icon}
Type=Application
Terminal=false
Categories={info.get('categories', 'Utility;')}
Comment=Managed by AppImage Manager
"""
        with open(desktop_file, 'w') as f:
            f.write(desktop_content)
            
        subprocess.run(['kbuildsycoca6'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        # Remove
        if os.path.exists(desktop_file):
            os.remove(desktop_file)
        if os.path.exists(icon_path):
            os.remove(icon_path)
            
        subprocess.run(['kbuildsycoca6'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def is_desktop_link_enabled(appimage_path):
    """Checks if a desktop link exists for this AppImage without reading its metadata."""
    desktop_dir = os.path.expanduser('~/.local/share/applications')
    original_name = os.path.basename(appimage_path)
    clean_name = _clean_name(original_name).replace('.AppImage', '')
    desktop_file = os.path.join(desktop_dir, f"appimage_{clean_name.replace(' ', '_').lower()}.desktop")
    return os.path.exists(desktop_file)

def find_app_corpses(appimage_path):
    """
    Finds related config and cache files in ~/.config, ~/.local/share, ~/.cache
    Returns a list of dicts: [{'path': str, 'size': int}, ...]
    """
    info = get_appimage_info(appimage_path)
    search_terms = [
        info['name'].lower(),
        info['clean_name'].replace('.AppImage', '').lower()
    ]
    
    # Clean up terms
    search_terms = list(set([term.strip() for term in search_terms if len(term.strip()) > 2]))
    
    dirs_to_search = [
        os.path.expanduser('~/.config'),
        os.path.expanduser('~/.local/share'),
        os.path.expanduser('~/.cache')
    ]
    
    corpses = []
    
    for base_dir in dirs_to_search:
        if not os.path.exists(base_dir):
            continue
            
        for item in os.listdir(base_dir):
            item_lower = item.lower()
            for term in search_terms:
                # Strict boundary matching: Must be preceded/followed by non-alphanumeric or start/end of string
                pattern = r'(?:^|[^a-z0-9])' + re.escape(term) + r'(?:[^a-z0-9]|$)'
                if re.search(pattern, item_lower):
                    full_path = os.path.join(base_dir, item)
                    # Calculate size
                    size = 0
                    if os.path.isdir(full_path):
                        for dirpath, _, filenames in os.walk(full_path):
                            for f in filenames:
                                fp = os.path.join(dirpath, f)
                                if not os.path.islink(fp):
                                    try:
                                        size += os.path.getsize(fp)
                                    except OSError:
                                        pass
                    else:
                        size = os.path.getsize(full_path)
                        
                    corpses.append({
                        'path': full_path,
                        'size': size
                    })
                    break # Found a match, no need to check other terms for this item
                    
    return corpses

def remove_items(appimage_path, corpse_paths):
    """
    Moves the specified corpse paths and the AppImage itself to the KDE Trash.
    """
    if "APPIMAGE_ITSELF" in corpse_paths:
        # 1. Disable desktop link (removes .desktop and icon)
        manage_desktop_link(appimage_path, enable=False)
        
        # 2. Move AppImage to trash
        if os.path.exists(appimage_path):
            subprocess.run(['kioclient', 'move', appimage_path, 'trash:/'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
    for p in corpse_paths:
        if p != "APPIMAGE_ITSELF" and os.path.exists(p):
            subprocess.run(['kioclient', 'move', p, 'trash:/'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
