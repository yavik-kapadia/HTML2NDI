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

# Code signing
echo ""
echo "Step 4: Code signing..."

# Check for Developer ID certificate
IDENTITY=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | sed 's/.*"\(.*\)".*/\1/')

if [ -n "$IDENTITY" ]; then
    echo "Signing with: $IDENTITY"
    
    WORKER="$RESOURCES/html2ndi.app"
    CEF="$WORKER/Contents/Frameworks/Chromium Embedded Framework.framework"
    
    # Sign NDI library
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$WORKER/Contents/Frameworks/libndi.dylib" 2>/dev/null
    
    # Sign CEF libraries
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$CEF/Libraries/libEGL.dylib" 2>/dev/null
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$CEF/Libraries/libGLESv2.dylib" 2>/dev/null
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$CEF/Libraries/libvk_swiftshader.dylib" 2>/dev/null
    
    # Sign CEF framework
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$CEF"
    
    # Sign helper apps
    for helper in "$WORKER/Contents/Frameworks/"*.app; do
        codesign --force --options runtime --timestamp --sign "$IDENTITY" "$helper"
    done
    
    # Sign worker app
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$WORKER"
    
    # Sign main app
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$APP_BUNDLE"
    
    echo "App signed with Developer ID"
else
    echo "No Developer ID found, using ad-hoc signing..."
    codesign --deep --force --sign - "$APP_BUNDLE" 2>/dev/null
fi

# Remove quarantine attribute
xattr -cr "$APP_BUNDLE" 2>/dev/null

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

