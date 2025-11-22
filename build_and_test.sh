#!/bin/bash

# Build test script for Pi5/Pi Zero 2W compatibility
# Run this on your Pi to verify the build works

set -e

echo "============================================"
echo "scanDoor Build Test"
echo "============================================"
echo ""

# Check libgpiod version
echo "Checking libgpiod installation..."
pkg-config --modversion libgpiodcxx libgpiod 2>/dev/null || echo "Note: pkg-config query skipped"
echo ""

# Clean build directory
echo "Cleaning build directory..."
rm -rf build
mkdir -p build
cd build

# Configure
echo "Running cmake..."
cmake ..
echo ""

# Build
echo "Building..."
make -j4
echo ""

echo "============================================"
echo "Build successful!"
echo "Executable: ./door_controller"
echo "============================================"
