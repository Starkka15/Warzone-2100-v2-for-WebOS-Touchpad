#!/bin/bash
# Package Warzone 2100 for webOS
# Creates an IPK file for installation via webOS App Manager

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WZ_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PKG_NAME="org.wz2100.warzone2100"
PKG_VERSION="2.3.0"
BUILD_DIR="$SCRIPT_DIR/build"
IPK_FILE="$SCRIPT_DIR/${PKG_NAME}_${PKG_VERSION}_arm.ipk"

echo "=== Packaging Warzone 2100 for webOS ==="
echo "Source: $WZ_ROOT"
echo "Output: $IPK_FILE"

# Clean previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/$PKG_NAME"

# Copy appinfo.json
cp "$SCRIPT_DIR/appinfo.json" "$BUILD_DIR/$PKG_NAME/"

# Copy icon (use existing warzone icon or create placeholder)
if [ -f "$WZ_ROOT/icons/warzone2100.png" ]; then
    # Resize to 64x64 for webOS
    if command -v convert >/dev/null 2>&1; then
        echo "Resizing icon to 64x64..."
        convert "$WZ_ROOT/icons/warzone2100.png" -resize 64x64 "$BUILD_DIR/$PKG_NAME/icon.png"
    else
        echo "Note: ImageMagick not found, using original icon size"
        cp "$WZ_ROOT/icons/warzone2100.png" "$BUILD_DIR/$PKG_NAME/icon.png"
    fi
elif [ -f "$SCRIPT_DIR/icon.png" ]; then
    cp "$SCRIPT_DIR/icon.png" "$BUILD_DIR/$PKG_NAME/"
else
    echo "Warning: No icon found, creating placeholder"
    # Create a simple placeholder (solid color)
    convert -size 64x64 xc:darkgreen "$BUILD_DIR/$PKG_NAME/icon.png" 2>/dev/null || \
    echo "Note: Install ImageMagick to generate placeholder icon"
fi

# Copy binary
echo "Copying binary..."
cp "$WZ_ROOT/src/warzone2100" "$BUILD_DIR/$PKG_NAME/"
chmod +x "$BUILD_DIR/$PKG_NAME/warzone2100"

# Copy game data
echo "Copying game data..."
mkdir -p "$BUILD_DIR/$PKG_NAME/data"
cp "$WZ_ROOT/data/base.wz" "$BUILD_DIR/$PKG_NAME/data/"
cp "$WZ_ROOT/data/mp.wz" "$BUILD_DIR/$PKG_NAME/data/"

# Copy mods (optional)
if [ -d "$WZ_ROOT/data/mods/multiplay" ]; then
    mkdir -p "$BUILD_DIR/$PKG_NAME/data/mods/multiplay"
    cp "$WZ_ROOT/data/mods/multiplay/"*.wz "$BUILD_DIR/$PKG_NAME/data/mods/multiplay/" 2>/dev/null || true
fi

# Create control file for IPK
mkdir -p "$BUILD_DIR/CONTROL"
cat > "$BUILD_DIR/CONTROL/control" << EOF
Package: $PKG_NAME
Version: $PKG_VERSION
Section: games
Priority: optional
Architecture: arm
Maintainer: Warzone 2100 Project
Description: Warzone 2100 - Real-time strategy game
 A free and open source real-time strategy game.
 Set in the aftermath of nuclear war, lead your forces
 to rebuild civilization.
EOF

cat > "$BUILD_DIR/CONTROL/postinst" << 'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$BUILD_DIR/CONTROL/postinst"

cat > "$BUILD_DIR/CONTROL/prerm" << 'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$BUILD_DIR/CONTROL/prerm"

# Create IPK (ar archive with control.tar.gz and data.tar.gz)
echo "Creating IPK..."
cd "$BUILD_DIR"

# Create data.tar.gz
tar czf data.tar.gz -C "$BUILD_DIR" "$PKG_NAME"

# Create control.tar.gz
tar czf control.tar.gz -C "$BUILD_DIR/CONTROL" .

# Create debian-binary
echo "2.0" > debian-binary

# Create IPK
ar -r "$IPK_FILE" debian-binary control.tar.gz data.tar.gz

# Cleanup
rm -f debian-binary control.tar.gz data.tar.gz

echo ""
echo "=== Package created: $IPK_FILE ==="
echo ""
echo "To install on TouchPad:"
echo "  1. Copy $IPK_FILE to device"
echo "  2. Install via Preware or:"
echo "     luna-send -n 1 palm://com.palm.appinstaller/installNoVerify '{\"target\":\"$IPK_FILE\"}'"
echo ""
echo "Or use novacom:"
echo "  novacom run file:///usr/bin/luna-send -- -n 1 palm://com.palm.appinstaller/installNoVerify '{\"target\":\"/media/internal/$(basename $IPK_FILE)\"}'"
