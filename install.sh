#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2024 AppImage Manager Contributors

set -e

# Colors for stdout formatting
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}==========================================${NC}"
echo -e "${BLUE}   AppImage Manager Installer Script     ${NC}"
echo -e "${BLUE}==========================================${NC}"

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: 'cmake' is not installed. Please install CMake and try again.${NC}"
    exit 1
fi

# Determine build preset
echo -e "${YELLOW}Select build mode:${NC}"
echo "1) Release (Recommended for standard usage)"
echo "2) Development (Includes debug symbols)"
read -rp "Enter choice [1-2, default: 1]: " choice

PRESET="release"
if [ "$choice" = "2" ]; then
    PRESET="dev"
fi

echo -e "\n${BLUE}[1/3] Configuring build with preset '$PRESET'...${NC}"
cmake --preset "$PRESET"

echo -e "\n${BLUE}[2/3] Building the targets...${NC}"
cmake --build --preset "$PRESET"

echo -e "\n${BLUE}[3/3] Installing to /usr (sudo/root access required)...${NC}"
sudo cmake --install "build/$PRESET"

# Ask to reload Dolphin
echo -e "\n${YELLOW}To register the Dolphin right-click context menu plugin, Dolphin needs to be restarted.${NC}"
read -rp "Would you like to restart Dolphin now? [Y/n, default: y]: " reload_dolphin

if [[ "$reload_dolphin" =~ ^[Yy]?$ ]] || [ -z "$reload_dolphin" ]; then
    echo -e "${BLUE}Restarting Dolphin...${NC}"
    if pgrep -x "dolphin" > /dev/null; then
        kquitapp6 dolphin 2>/dev/null || killall dolphin 2>/dev/null || true
        sleep 1
        dolphin &>/dev/null &
        disown
        echo -e "${GREEN}Dolphin restarted successfully.${NC}"
    else
        echo "Dolphin is not currently running. The plugin will load the next time Dolphin is launched."
    fi
fi

echo -e "\n${GREEN}AppImage Manager has been successfully installed!${NC}"
