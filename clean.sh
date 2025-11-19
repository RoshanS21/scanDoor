#!/bin/bash

# Stop any running instances
sudo pkill -f door_controller

# Remove build directory
rm -rf build/

# Remove generated log files
rm -rf logs/*.log

# Remove any core dumps
rm -f core.*

echo "Clean-up complete. Build directory and logs removed."
echo
echo "# Create build directory and build"
echo "mkdir build"
echo "cd build"
echo "cmake .."
echo "make -j4"
echo
