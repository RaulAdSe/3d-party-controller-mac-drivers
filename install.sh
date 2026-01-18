#!/bin/bash
# install.sh - Install the Bigben Controller Mapper for macOS

set -e

echo "========================================"
echo "  Bigben Controller Mapper Installer"
echo "========================================"
echo ""

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "Error: Homebrew is required but not installed."
    echo "Install it from https://brew.sh"
    exit 1
fi

# Install libusb
echo "Installing libusb..."
brew install libusb 2>/dev/null || brew upgrade libusb 2>/dev/null || true

# Check if Swift is available
if ! command -v swift &> /dev/null; then
    echo "Error: Swift is required but not installed."
    echo "Install Xcode or Xcode Command Line Tools."
    exit 1
fi

# Build the project
echo ""
echo "Building the mapper..."
cd "$(dirname "$0")"
swift build -c release 2>&1 | grep -v "^warning:" || true

# Create installation directory
INSTALL_DIR="$HOME/.local/bin"
mkdir -p "$INSTALL_DIR"

# Copy binary
echo ""
echo "Installing to $INSTALL_DIR..."
cp .build/release/bigben-mapper "$INSTALL_DIR/"

# Check if in PATH
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo ""
    echo "Note: $INSTALL_DIR is not in your PATH."
    echo "Add this to your shell profile (e.g., ~/.zshrc):"
    echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
fi

echo ""
echo "========================================"
echo "  Installation Complete!"
echo "========================================"
echo ""
echo "Usage:"
echo "  bigben-mapper          - Start the controller mapper"
echo "  bigben-mapper --debug  - Start with debug output"
echo ""
echo "Note: Grant Accessibility permission when prompted."
echo "      System Settings → Privacy & Security → Accessibility"
echo ""
echo "Controller Mapping (FPS preset):"
echo "  Left Stick  → W/A/S/D (Movement)"
echo "  Right Stick → Mouse (Look around)"
echo "  A           → Space (Jump)"
echo "  B           → C (Crouch)"
echo "  X           → R (Reload)"
echo "  Y           → F (Interact)"
echo "  LB/RB       → Q/E"
echo "  LT/RT       → Right/Left Mouse Click"
echo "  D-Pad       → 1/2/3/4"
echo ""
