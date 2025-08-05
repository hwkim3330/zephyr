#!/bin/bash
# Raspberry Pi 4B TSN Build Fix Script
# 빌드 에러를 해결하는 간단한 스크립트

echo "🔧 Fixing Raspberry Pi 4B TSN build issues..."

# 1. 스크립트 권한 수정
chmod +x scripts/setup_rpi_env.sh

# 2. 시스템 Python 사용 강제
export PYTHON_EXECUTABLE=/usr/bin/python3
export PYTHON3_EXECUTABLE=/usr/bin/python3
export WEST_PYTHON=/usr/bin/python3

# 3. PATH에서 Miniconda 제거하고 시스템 경로 우선
export PATH="/usr/bin:/bin:/usr/local/bin:$PATH"

# Miniconda 경로를 PATH에서 완전히 제거
export PATH=$(echo $PATH | tr ':' '\n' | grep -v conda | tr '\n' ':' | sed 's/:$//')

echo "✅ Python environment fixed:"
echo "   PYTHON_EXECUTABLE: $PYTHON_EXECUTABLE"
echo "   WEST_PYTHON: $WEST_PYTHON"
echo "   PATH: $(echo $PATH | cut -c1-80)..."

# 4. West 재초기화
echo "🔄 Reinitializing west workspace..."
rm -rf .west build
west init -l .
west update --quiet

# 5. Python 요구사항 설치 (시스템 Python 사용)
echo "📦 Installing Python requirements..."
/usr/bin/python3 -m pip install --user --quiet west PyYAML pyelftools

# 6. Clean build with fixed environment
echo "🚀 Starting clean build..."
west build -p always -b rpi_4b samples/net/tsn/raspberry_pi_4b

if [ $? -eq 0 ]; then
    echo "🎉 Build successful!"
    echo "   Binary location: build/zephyr/zephyr.bin"
    echo "   Ready to flash to Raspberry Pi 4B!"
else
    echo "❌ Build failed. Check the error messages above."
    exit 1
fi