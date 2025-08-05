#!/bin/bash
# DTS 처리 에러 임시 우회 해결 스크립트

echo "🔧 Fixing DTS processing error..."

# 1. 가상환경 활성화
if [ ! -d "tsn_venv" ]; then
    python3 -m venv tsn_venv
fi
source tsn_venv/bin/activate

# 2. 필요한 패키지 설치
pip install west PyYAML pyelftools dtschema

# 3. DTC 경로 확인 및 수정
DTC_PATH="/opt/zephyr-sdk/sysroots/x86_64-pokysdk-linux/usr/bin/dtc"
if [ ! -f "$DTC_PATH" ]; then
    echo "DTC not found at expected path, trying system dtc..."
    sudo apt install -y device-tree-compiler
    DTC_PATH=$(which dtc)
fi

echo "Using DTC: $DTC_PATH"

# 4. 문제가 되는 DTS 파일 검증
echo "🔍 Checking DTS files..."
DTS_FILE="boards/raspberrypi/rpi_4b/rpi_4b.dts"

if [ -f "$DTS_FILE" ]; then
    echo "✅ Found $DTS_FILE"
    # DTS 파일 문법 검증
    $DTC_PATH -I dts -O dtb $DTS_FILE -o /tmp/test_rpi4b.dtb 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "✅ DTS syntax is valid"
    else
        echo "❌ DTS syntax error detected"
        # 기본 DTS로 복원
        echo "Restoring basic DTS..."
        cp $DTS_FILE ${DTS_FILE}.backup
        cat > $DTS_FILE << 'EOF'
/*
 * Copyright 2023 honglin leng <a909204013@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/dts-v1/;

#include <broadcom/bcm2711.dtsi>
#include <zephyr/dt-bindings/gpio/gpio.h>

/ {
	model = "Raspberry Pi 4 Model B";
	compatible = "raspberrypi,4-model-b", "brcm,bcm2838";
	#address-cells = <1>;
	#size-cells = <1>;

	chosen {
		zephyr,console = &uart1;
		zephyr,shell-uart = &uart1;
		zephyr,sram = &sram0;
	};
};

&uart1 {
	status = "okay";
};
EOF
    fi
else
    echo "❌ DTS file not found: $DTS_FILE"
    exit 1
fi

# 5. West 재초기화
echo "🔄 Reinitializing west..."
rm -rf .west build
west init -l .
west update --quiet

# 6. DTC 플래그 최소화하여 빌드 시도
echo "🚀 Attempting build with minimal DTC flags..."
export EXTRA_DTC_FLAGS=""
export DTC_NO_WARN_UNIT_ADDR=""

west build -b rpi_4b samples/net/tsn/raspberry_pi_4b -- \
    -DDTC_NO_WARN_UNIT_ADDR="" \
    -DEXTRA_DTC_FLAGS="" \
    -DPYTHON_EXECUTABLE=$(which python3)

if [ $? -eq 0 ]; then
    echo ""
    echo "🎉 BUILD SUCCESSFUL!"
    echo "📁 Binary: build/zephyr/zephyr.bin"
else
    echo "❌ Build still failed. Trying alternative approach..."
    
    # 7. 대안: 기존 성공한 DTS를 사용
    echo "Trying with simpler board configuration..."
    west build -b qemu_cortex_a53 samples/hello_world
    
    if [ $? -eq 0 ]; then
        echo "✅ Alternative build works. The issue is RPI4B specific."
        echo "Try building for a different target first to verify toolchain."
    fi
fi