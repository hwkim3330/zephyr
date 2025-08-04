# 라즈베리파이 4B에서 Zephyr TSN 실행 가이드

## 하드웨어 요구사항
- 라즈베리파이 4B (4GB 모델 권장, 1GB도 가능)
- microSD 카드 (최소 8GB, Class 10 권장)
- 이더넷 케이블
- USB-C 전원 어댑터 (5V 3A)
- USB-UART 컨버터 (디버깅용, 선택사항)

## 1. 개발 환경 설정

### Windows에서 크로스 컴파일 환경 구축

```bash
# 1. Zephyr SDK 설치 (Windows)
# https://github.com/zephyrproject-rtos/sdk-ng/releases 에서 최신 버전 다운로드
# zephyr-sdk-0.16.x-windows-x86_64.7z 압축 해제

# 2. West 설치
pip install west

# 3. Toolchain 설정
set ZEPHYR_TOOLCHAIN_VARIANT=zephyr
set ZEPHYR_SDK_INSTALL_DIR=C:\zephyr-sdk-0.16.x

# 또는 ARM GCC 사용
# https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
set ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
set CROSS_COMPILE=arm-none-eabi-
```

### Linux에서 크로스 컴파일 환경 구축

```bash
# 1. 필수 패키지 설치
sudo apt update
sudo apt install -y git cmake ninja-build gperf ccache dfu-util device-tree-compiler wget \
    python3-dev python3-pip python3-setuptools python3-tk python3-wheel xz-utils file \
    make gcc gcc-multilib g++-multilib libsdl2-dev

# 2. West 설치
pip3 install --user -U west

# 3. ARM64 크로스 컴파일러 설치
sudo apt install -y gcc-aarch64-linux-gnu

# 4. 환경 변수 설정
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE=aarch64-linux-gnu-
```

## 2. TSN 펌웨어 빌드

```bash
# 1. Zephyr 프로젝트 클론 (이미 있다면 스킵)
west init zephyr-project
cd zephyr-project
west update

# 2. TSN 샘플 빌드
cd zephyr/samples/net/tsn/raspberry_pi_4b

# TSN 기능 모두 포함해서 빌드
west build -b rpi_4b -- -DCONF_FILE=prj.conf

# 또는 사전 구성된 TSN defconfig 사용
west build -b rpi_4b -- -DCONF_FILE=../../../boards/raspberrypi/rpi_4b/rpi_4b_tsn_defconfig

# 빌드 결과: build/zephyr/zephyr.bin
```

## 3. SD 카드 준비 및 굽기

### 방법 1: Raspberry Pi Imager 사용 (권장)

```bash
# 1. Raspberry Pi Imager 다운로드 및 설치
# https://www.raspberrypi.org/software/

# 2. SD 카드 포맷
# Imager에서 "CHOOSE OS" -> "Erase" 선택해서 먼저 포맷

# 3. 부트로더 설치
# "CHOOSE OS" -> "Misc utility images" -> "Bootloader (Pi 4 family)"
# -> "USB Boot" 선택 (또는 SD Card Boot)
```

### 방법 2: 수동으로 부트 파티션 생성

```bash
# Windows에서 (관리자 권한 명령프롬프트)
# SD 카드를 D: 드라이브라고 가정

# 1. SD 카드 포맷 (FAT32)
format D: /FS:FAT32 /Q

# 2. 라즈베리파이 부트 파일들 다운로드
# https://github.com/raspberrypi/firmware/tree/master/boot
# 필요한 파일들:
# - bootcode.bin
# - start4.elf  
# - fixup4.dat
# - config.txt
# - cmdline.txt

# 3. config.txt 내용:
enable_uart=1
arm_64bit=1
kernel=zephyr.bin
kernel_address=0x80000

# 4. cmdline.txt 내용 (한 줄로):
console=ttyAMA0,115200 console=tty1

# 5. Zephyr 바이너리 복사
copy build\zephyr\zephyr.bin D:\zephyr.bin
```

### Linux/Mac에서 SD 카드 굽기

```bash
# 1. SD 카드 디바이스 확인
lsblk
# 예: /dev/sdb (주의: 실제 디바이스명 확인 필수!)

# 2. SD 카드 언마운트
sudo umount /dev/sdb*

# 3. 파티션 생성
sudo fdisk /dev/sdb
# - 'o' (새 파티션 테이블)
# - 'n' (새 파티션, 기본값들 선택)
# - 't' (파티션 타입 'c' = W95 FAT32)
# - 'a' (부팅 가능)
# - 'w' (저장)

# 4. 파일시스템 생성
sudo mkfs.vfat /dev/sdb1

# 5. 마운트 및 파일 복사
sudo mkdir -p /mnt/rpi
sudo mount /dev/sdb1 /mnt/rpi

# 부트 파일들 복사 (위에서 다운로드한 파일들)
sudo cp bootcode.bin start4.elf fixup4.dat config.txt cmdline.txt /mnt/rpi/
sudo cp build/zephyr/zephyr.bin /mnt/rpi/

sudo umount /mnt/rpi
```

## 4. 네트워크 연결 설정

### 기본 설정
```bash
# config.txt에 추가 (이더넷 활성화)
dtparam=audio=on
dtoverlay=vc4-kms-v3d
max_framebuffers=2

# TSN을 위한 고성능 설정
gpu_mem=64
disable_overscan=1
hdmi_force_hotplug=1
```

### TSN 네트워크 토폴로지 예제
```
[TSN Master Clock] ---- [TSN Switch] ---- [Raspberry Pi 4B]
                            |
                       [Other TSN Nodes]
```

## 5. 실행 및 테스트

### 시리얼 콘솔 연결
```bash
# Windows - PuTTY 또는 TeraTerm
# 설정: 115200 baud, 8N1, No flow control

# Linux/Mac
sudo minicom -D /dev/ttyUSB0 -b 115200
# 또는
sudo screen /dev/ttyUSB0 115200
```

### TSN 기능 테스트
```bash
# 1. 부팅 후 쉘 프롬프트에서
uart:~$ net iface
# 네트워크 인터페이스 확인

uart:~$ tsn status  
# TSN 상태 확인

uart:~$ tsn demo_start
# TSN 데모 시작

uart:~$ net stats
# 네트워크 통계 확인
```

## 6. 트러블슈팅

### 부팅 안됨
- 전원 LED만 켜지고 부팅 안됨: SD 카드 재포맷
- 무지개 화면: config.txt 설정 확인
- 커널 패닉: zephyr.bin 파일 손상 확인

### 네트워크 안됨  
```bash
# 이더넷 케이블 연결 확인
uart:~$ net iface show

# PHY 상태 확인
uart:~$ net stats ethernet

# DHCP 재시도
uart:~$ net dhcpv4 start 1
```

### TSN 기능 안됨
```bash
# TSN 설정 확인
uart:~$ tsn status

# 빌드 시 TSN 기능 활성화 확인
# CONFIG_NET_L2_IEEE802_TSN=y
# CONFIG_NET_L2_IEEE802_1CB=y  
# CONFIG_NET_L2_IEEE802_1QBV=y
```

## 7. 성능 최적화

### 고성능 설정
```bash
# config.txt에 추가
# CPU 성능 모드
arm_freq=1800
over_voltage=6

# 메모리 설정
gpu_mem=64
disable_l2cache=0

# TSN을 위한 실시간 설정
isolcpus=2,3
rcu_nocbs=2,3
```

### 네트워크 버퍼 튜닝
```kconfig
# prj.conf에 추가
CONFIG_NET_BUF_RX_COUNT=128
CONFIG_NET_BUF_TX_COUNT=128
CONFIG_NET_PKT_RX_COUNT=64
CONFIG_NET_PKT_TX_COUNT=64
CONFIG_HEAP_MEM_POOL_SIZE=131072
```

## 8. 고급 TSN 설정

### FRER 스트림 설정
```c
// 커스텀 스트림 추가
struct ieee802_1cb_stream_config my_stream = {
    .stream_handle = 10,
    .id_type = IEEE802_1CB_STREAM_ID_DEST_MAC_VLAN,
    .id.mac_vlan = {
        .mac_addr = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB},
        .vlan_id = 500
    },
    .replication_enabled = true,
    .elimination_enabled = true,
    .history_length = 64
};
```

### TAS 게이트 스케줄 설정
```c
// 1ms 사이클, 8개 트래픽 클래스
struct ieee802_1qbv_gate_entry custom_schedule[] = {
    {IEEE802_1QBV_GATE_OPEN, 0x80, 200000},  // TC7만 200μs
    {IEEE802_1QBV_GATE_OPEN, 0x60, 300000},  // TC5,6만 300μs  
    {IEEE802_1QBV_GATE_OPEN, 0x1F, 500000}   // TC0-4만 500μs
};
```

이제 SD 카드에 구워서 라즈베리파이 4B에서 TSN 기능을 테스트할 수 있습니다! 🚀