# 🔌 USB-C Serial Console for Raspberry Pi 4B TSN Demo

## ✅ **USB-C 시리얼 콘솔 설정 완료!**

이제 GPIO UART 핀 연결 없이 **USB-C 케이블 하나**로 라즈베리파이 4B와 PC를 연결해서 시리얼 콘솔을 사용할 수 있습니다.

---

## 🚀 **빌드 방법**

### 1. 라즈베리파이 4B용 빌드:
```bash
west build -b rpi_4b samples/net/tsn_shell_demo
```

### 2. QEMU로 테스트 (개발용):
```bash
west build -b qemu_cortex_a53 samples/net/tsn_shell_demo
```

---

## 🔌 **USB-C 연결 방법**

### 1. **하드웨어 연결**
- 라즈베리파이 4B의 **USB-C 포트** (전원 포트)
- PC의 USB-A 또는 USB-C 포트
- **USB-C to USB-A 케이블** 또는 **USB-C to USB-C 케이블**

### 2. **PC에서 장치 확인**

**Linux:**
```bash
ls /dev/ttyACM* 
# 또는
dmesg | grep tty
```

**Windows:**
- 장치 관리자 → 포트(COM 및 LPT) → "TSN_RPI4B_DEMO" 확인

**macOS:**
```bash
ls /dev/tty.usbmodem*
```

---

## 💻 **시리얼 터미널 연결**

### Linux (screen 사용):
```bash
screen /dev/ttyACM0
```

### Windows (PuTTY):
1. PuTTY 실행
2. Connection Type: Serial
3. Serial Line: COM포트번호 (예: COM3)
4. Speed: 115200 (또는 자동)

### macOS (screen 사용):
```bash
screen /dev/tty.usbmodem* 115200
```

---

## 🎯 **TSN 명령어 테스트**

연결 후 다음 명령어들을 테스트해보세요:

### 1. **USB-C 연결 테스트**
```bash
usbtest
```
→ USB-C 시리얼 콘솔이 제대로 작동하는지 확인

### 2. **TSN 시스템 상태**
```bash
tsn status
```
→ 라즈베리파이 4B 하드웨어 정보 및 TSN 기능 상태 표시

### 3. **TSN 데모 실행**
```bash
tsn demo
```
→ IEEE 802.1CB/Qbv TSN 기능 데모

### 4. **시스템 정보**
```bash
kernel version
kernel uptime
help
```

---

## ⚙️ **설정 파일 설명**

### `prj.conf` - USB-C CDC ACM 설정:
```ini
CONFIG_USB_DEVICE_STACK=y        # USB 디바이스 스택 활성화
CONFIG_USB_CDC_ACM=y             # CDC ACM (가상 COM 포트) 활성화
CONFIG_USB_UART_CONSOLE=y        # USB를 콘솔로 사용
CONFIG_UART_CONSOLE=n            # GPIO UART 비활성화
CONFIG_USB_DEVICE_PRODUCT="TSN_RPI4B_DEMO"  # PC에서 보이는 장치명
```

### `boards/rpi_4b.overlay` - Device Tree 설정:
```dts
chosen {
    zephyr,console = &cdc_acm_uart0;    # USB CDC ACM을 콘솔로 지정
    zephyr,shell-uart = &cdc_acm_uart0; # 쉘도 USB CDC ACM 사용
};
```

---

## 🔧 **장점**

✅ **케이블 하나로 해결** - USB-C 케이블 하나로 전원 + 시리얼 콘솔  
✅ **핀 연결 불필요** - GPIO TX/RX 핀 연결 작업 없음  
✅ **속도 빠름** - USB 2.0 속도 (가상 시리얼)  
✅ **자동 인식** - PC에서 드라이버 자동 설치  
✅ **깔끔한 연결** - 추가 어댑터나 변환기 불필요  

---

## 🎉 **완성!**

이제 **USB-C 케이블 하나**로 라즈베리파이 4B의 TSN 기능을 완전히 제어할 수 있습니다!

**TSN_RPI4B_DEMO** 장치로 연결해서 `tsn status`, `tsn demo`, `usbtest` 명령어를 즐겨보세요! 🚀