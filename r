#!/bin/bash
# Script to run the Vulkan compute shader program

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MAIN_EXECUTABLE="$SCRIPT_DIR/bin/main"

# Check if the executable exists
if [ ! -f "$MAIN_EXECUTABLE" ]; then
    echo "Error: Executable not found at $MAIN_EXECUTABLE"
    echo "Please run ./m to build the project first."
    exit 1
fi

# Run the program
echo "Running Vulkan compute shader program..."
echo "----------------------------------------"
"$MAIN_EXECUTABLE" "$@"
