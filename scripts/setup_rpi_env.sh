#!/bin/bash
# Raspberry Pi 4B TSN Environment Setup Script
# Ensures proper Python environment for building TSN on Raspberry Pi 4B

set -e

echo "Setting up Raspberry Pi 4B TSN build environment..."

# Use system Python instead of Miniconda to avoid conflicts
export PYTHON_EXECUTABLE=/usr/bin/python3
export PATH="/usr/bin:$PATH"

# Verify Python version
PYTHON_VERSION=$(python3 --version 2>&1 | grep -oE '[0-9]+\.[0-9]+')
echo "Using Python version: $PYTHON_VERSION"

# Check for required Python version (3.8+)
if ! python3 -c "import sys; assert sys.version_info >= (3, 8)"; then
    echo "ERROR: Python 3.8 or newer is required"
    exit 1
fi

# Install/upgrade pip if needed
if ! python3 -m pip --version > /dev/null 2>&1; then
    echo "Installing pip..."
    curl https://bootstrap.pypa.io/get-pip.py | python3
fi

# Install requirements
echo "Installing Python dependencies..."
python3 -m pip install --user -r scripts/requirements-rpi.txt

# Install west if not available
if ! which west > /dev/null 2>&1; then
    echo "Installing west build tool..."
    python3 -m pip install --user west
fi

# Initialize west workspace if needed
if [ ! -f .west/config ]; then
    echo "Initializing west workspace..."
    west init -l .
fi

# Update west modules
echo "Updating west modules..."
west update

echo "Environment setup complete!"
echo "You can now build the TSN sample with:"
echo "  west build -p always -b rpi_4b samples/net/tsn/raspberry_pi_4b"