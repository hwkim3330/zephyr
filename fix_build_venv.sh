#!/bin/bash
# Raspberry Pi 4B TSN Build Fix Script with Virtual Environment
# Ubuntu externally-managed-environment 해결

set -e

echo "🔧 Setting up virtual environment for Raspberry Pi 4B TSN build..."

# 1. 시스템 패키지 설치 (필요한 경우)
echo "📦 Installing system packages..."
sudo apt update -qq
sudo apt install -y python3-full python3-venv python3-pip cmake ninja-build device-tree-compiler

# 2. 가상환경 생성 및 활성화
VENV_DIR="tsn_venv"
if [ ! -d "$VENV_DIR" ]; then
    echo "🐍 Creating Python virtual environment..."
    python3 -m venv $VENV_DIR
fi

echo "⚡ Activating virtual environment..."
source $VENV_DIR/bin/activate

# 3. Python 패키지 설치
echo "📦 Installing Python requirements in virtual environment..."
pip install --upgrade pip
pip install west PyYAML pyelftools kconfiglib junitparser psutil colorama canopen

# 4. 환경 변수 설정
export PYTHON_EXECUTABLE=$(which python3)
export WEST_PYTHON=$(which python3)
export Python3_EXECUTABLE=$(which python3)

echo "✅ Virtual environment setup complete:"
echo "   Virtual env: $VIRTUAL_ENV"
echo "   Python: $(which python3)"
echo "   West: $(which west)"

# 5. West 초기화
echo "🔄 Initializing west workspace..."
if [ -d ".west" ]; then
    rm -rf .west
fi
if [ -d "build" ]; then
    rm -rf build
fi

west init -l .
west update --quiet

# 6. 빌드 실행
echo "🚀 Starting TSN build..."
west build -p always -b rpi_4b samples/net/tsn/raspberry_pi_4b

if [ $? -eq 0 ]; then
    echo ""
    echo "🎉 BUILD SUCCESSFUL!"
    echo "📁 Binary location: build/zephyr/zephyr.bin"
    echo "📏 Binary size: $(ls -lh build/zephyr/zephyr.bin | awk '{print $5}')"
    echo ""
    echo "🔧 To flash to Raspberry Pi 4B SD card:"
    echo "   sudo dd if=build/zephyr/zephyr.bin of=/dev/sdX bs=4M status=progress"
    echo "   (Replace /dev/sdX with your SD card device)"
    echo ""
    echo "💡 To use this build environment again:"
    echo "   source tsn_venv/bin/activate"
    echo "   west build samples/net/tsn/raspberry_pi_4b"
else
    echo "❌ Build failed. Check error messages above."
    exit 1
fi