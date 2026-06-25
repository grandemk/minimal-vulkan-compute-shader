#!/bin/bash
# Script to compile the Vulkan compute shader project with DXC and HLSL

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/bin"

# Create build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory: $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
fi

# Run cmake
echo "Running cmake..."
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

# Build with make
echo "Building project..."
make

echo "Build complete! Run with ./r or cd bin && ./main"
