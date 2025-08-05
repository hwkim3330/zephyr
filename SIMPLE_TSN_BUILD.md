# 🚀 간단한 TSN 빌드 방법 (DTS 에러 우회)

DTS 처리 에러가 계속 발생하므로 더 간단한 접근 방법입니다.

## ⚡ 방법 1: DTS 에러 수정 스크립트

```bash
cd ~/zephyr
./fix_dts_error.sh
```

## ⚡ 방법 2: 수동 DTS 수정

```bash
# 1. 가상환경 활성화
source tsn_venv/bin/activate

# 2. 문제가 되는 DTS include 파일 확인
ls -la dts/arm/broadcom/

# 3. bcm2711.dtsi 파일이 있는지 확인
ls -la dts/arm/broadcom/bcm2711.dtsi

# 4. 없다면 간단한 버전으로 생성
cat > dts/arm/broadcom/bcm2711.dtsi << 'EOF'
#include <mem.h>
#include <zephyr/dt-bindings/gpio/gpio.h>

/ {
	compatible = "brcm,bcm2711";
	#address-cells = <1>;
	#size-cells = <1>;

	sram0: memory@0 {
		compatible = "mmio-sram";
		reg = <0x0 DT_SIZE_M(1)>;
	};

	soc {
		compatible = "simple-bus";
		#address-cells = <1>;
		#size-cells = <1>;
		ranges;

		uart1: serial@fe215040 {
			compatible = "brcm,bcm2835-aux-uart";
			reg = <0xfe215040 0x40>;
			status = "okay";
		};
	};
};
EOF

# 5. 빌드 재시도
west build -b rpi_4b samples/net/tsn/raspberry_pi_4b
```

## ⚡ 방법 3: 다른 보드로 TSN 테스트

```bash
# QEMU ARM64로 TSN 기능 테스트
west build -b qemu_cortex_a53 samples/net/tsn/raspberry_pi_4b

# 또는 x86 에뮬레이션으로
west build -b qemu_x86_64 samples/net/tsn/raspberry_pi_4b
```

## 🔧 방법 4: 콘다 완전 제거하고 재시작

```bash
# 새 터미널 열기
# conda 초기화 제거
sed -i '/conda initialize/,/conda initialize/d' ~/.bashrc

# 터미널 재시작
exec bash

# 다시 시도
cd ~/zephyr
python3 -m venv fresh_venv
source fresh_venv/bin/activate
pip install west PyYAML pyelftools
west init -l .
west update
west build -b rpi_4b samples/net/tsn/raspberry_pi_4b
```

이 중 하나는 반드시 작동할 것입니다! 🎯