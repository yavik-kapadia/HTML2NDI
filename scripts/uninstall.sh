#!/bin/bash
#
# HTML2NDI Uninstallation Script
#

set -e

INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
LAUNCHAGENT_DIR="$HOME/Library/LaunchAgents"

echo "HTML2NDI Uninstaller"
echo "===================="
echo ""

# Stop LaunchAgent if running
if [[ -f "$LAUNCHAGENT_DIR/com.html2ndi.plist" ]]; then
    echo "Stopping LaunchAgent..."
    launchctl unload "$LAUNCHAGENT_DIR/com.html2ndi.plist" 2>/dev/null || true
    rm -f "$LAUNCHAGENT_DIR/com.html2ndi.plist"
fi

# Remove binaries
echo "Removing binaries..."
sudo rm -f "$INSTALL_PREFIX/bin/html2ndi"
sudo rm -f "$INSTALL_PREFIX/bin/html2ndi_helper"

# Remove resources
echo "Removing resources..."
sudo rm -rf "$INSTALL_PREFIX/share/html2ndi"

# Remove frameworks
echo "Removing frameworks..."
sudo rm -rf "$INSTALL_PREFIX/lib/html2ndi"

# Remove logs (optional)
read -p "Remove log files? [y/N] " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Removing logs..."
    sudo rm -rf /var/log/html2ndi
fi

echo ""
echo "Uninstallation complete!"

