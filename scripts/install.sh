#!/bin/bash
#
# HTML2NDI Installation Script
#

set -e

# Configuration
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
LAUNCHAGENT_DIR="$HOME/Library/LaunchAgents"

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

echo "HTML2NDI Installer"
echo "=================="
echo ""

# Check if build exists
if [[ ! -f "$BUILD_DIR/bin/html2ndi" ]]; then
    echo "Error: html2ndi not found. Please build first:"
    echo ""
    echo "  mkdir -p build && cd build"
    echo "  cmake .."
    echo "  cmake --build ."
    echo ""
    exit 1
fi

# Install binaries
echo "Installing binaries to $INSTALL_PREFIX/bin..."
sudo mkdir -p "$INSTALL_PREFIX/bin"
sudo cp "$BUILD_DIR/bin/html2ndi" "$INSTALL_PREFIX/bin/"
sudo cp "$BUILD_DIR/bin/html2ndi_helper" "$INSTALL_PREFIX/bin/"
sudo chmod +x "$INSTALL_PREFIX/bin/html2ndi"
sudo chmod +x "$INSTALL_PREFIX/bin/html2ndi_helper"

# Install resources
echo "Installing resources to $INSTALL_PREFIX/share/html2ndi..."
sudo mkdir -p "$INSTALL_PREFIX/share/html2ndi"
sudo cp "$PROJECT_DIR/resources/test.html" "$INSTALL_PREFIX/share/html2ndi/"

# Install CEF framework
if [[ -d "$BUILD_DIR/bin/Frameworks" ]]; then
    echo "Installing CEF framework..."
    sudo mkdir -p "$INSTALL_PREFIX/lib/html2ndi/Frameworks"
    sudo cp -R "$BUILD_DIR/bin/Frameworks/"* "$INSTALL_PREFIX/lib/html2ndi/Frameworks/"
fi

# Copy CEF resources
if [[ -d "$BUILD_DIR/bin/Resources" ]]; then
    echo "Installing CEF resources..."
    sudo cp -R "$BUILD_DIR/bin/Resources" "$INSTALL_PREFIX/share/html2ndi/"
fi

# Copy NDI library if present
if [[ -f "$BUILD_DIR/bin/libndi.dylib" ]]; then
    echo "Installing NDI library..."
    sudo cp "$BUILD_DIR/bin/libndi.dylib" "$INSTALL_PREFIX/lib/"
fi

# Create log directory
echo "Creating log directory..."
sudo mkdir -p /var/log/html2ndi
sudo chmod 755 /var/log/html2ndi

# Install LaunchAgent (optional)
echo ""
read -p "Install LaunchAgent for auto-start? [y/N] " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Installing LaunchAgent..."
    mkdir -p "$LAUNCHAGENT_DIR"
    cp "$PROJECT_DIR/resources/com.html2ndi.plist" "$LAUNCHAGENT_DIR/"
    
    # Update paths in plist
    sed -i '' "s|/usr/local|$INSTALL_PREFIX|g" "$LAUNCHAGENT_DIR/com.html2ndi.plist"
    
    echo ""
    echo "To enable auto-start:"
    echo "  launchctl load $LAUNCHAGENT_DIR/com.html2ndi.plist"
    echo ""
    echo "To disable:"
    echo "  launchctl unload $LAUNCHAGENT_DIR/com.html2ndi.plist"
fi

echo ""
echo "Installation complete!"
echo ""
echo "Usage:"
echo "  html2ndi --url https://example.com"
echo "  html2ndi --help"
echo ""
echo "Test page:"
echo "  html2ndi --url file://$INSTALL_PREFIX/share/html2ndi/test.html"

