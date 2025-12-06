#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build"

echo "=== Building HTML2NDI Manager ==="

# Build the C++ worker first
echo ""
echo "Step 1: Building html2ndi worker..."
cd "$PROJECT_DIR"
if [ ! -d "build" ]; then
    mkdir build
    cd build
    cmake ..
else
    cd build
fi
cmake --build . -j8

echo ""
echo "Step 2: Building Swift manager app..."
cd "$SCRIPT_DIR"

# Build using swift build
swift build -c release

# Create app bundle structure
APP_NAME="HTML2NDI Manager"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
CONTENTS="$APP_BUNDLE/Contents"
MACOS="$CONTENTS/MacOS"
RESOURCES="$CONTENTS/Resources"

rm -rf "$APP_BUNDLE"
mkdir -p "$MACOS" "$RESOURCES"

# Copy executable
cp ".build/release/HTML2NDI Manager" "$MACOS/"

# Copy Info.plist
cp "HTML2NDI Manager/Info.plist" "$CONTENTS/"

# Copy app icon
cp "Resources/AppIcon.icns" "$RESOURCES/"

# Copy the html2ndi worker app bundle
echo ""
echo "Step 3: Bundling html2ndi worker..."
cp -R "$PROJECT_DIR/build/bin/html2ndi.app" "$RESOURCES/"

echo ""
echo "=== Build Complete ==="
echo ""
echo "App bundle: $APP_BUNDLE"
echo ""
echo "To install:"
echo "  cp -R \"$APP_BUNDLE\" /Applications/"
echo ""
echo "To run:"
echo "  open \"$APP_BUNDLE\""

