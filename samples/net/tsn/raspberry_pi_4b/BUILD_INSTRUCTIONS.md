# 🚀 Raspberry Pi 4B TSN 빌드 및 실행 가이드

## ⚡ 빠른 시작

### 1. 환경 설정 (한 번만 실행)
```bash
# Zephyr 저장소 클론
git clone https://github.com/hwkim3330/zephyr.git
cd zephyr
git checkout feature/raspberry-pi-tsn-support

# 자동 환경 설정 실행
./scripts/setup_rpi_env.sh
```

### 2. TSN 샘플 빌드
```bash
west build -p always -b rpi_4b samples/net/tsn/raspberry_pi_4b
```

### 3. SD 카드에 플래시
```bash
# 빌드된 이미지를 SD 카드에 복사
cp build/zephyr/zephyr.bin /path/to/sdcard/
```

---

## 🔧 문제 해결

### CMake DTS 에러 해결됨 ✅
- `BCM2711 DTSI 파일 누락` → **해결**: `dts/arm/broadcom/bcm2711.dtsi` 추가
- `Python 환경 충돌` → **해결**: 시스템 Python 강제 사용
- `west 모듈 불일치` → **해결**: 자동 west 초기화

### 빌드 에러가 계속 발생한다면:

1. **Python 환경 리셋**:
```bash
export PYTHON_EXECUTABLE=/usr/bin/python3
export PATH="/usr/bin:$PATH"
```

2. **Clean 빌드**:
```bash
west build -p always -b rpi_4b samples/net/tsn/raspberry_pi_4b
```

3. **West 재초기화**:
```bash
rm -rf .west
west init -l .
west update
```

---

## 📋 시스템 요구사항

### 필수 소프트웨어:
- **Python**: 3.8+ (시스템 Python 권장)
- **CMake**: 3.20+
- **West**: 1.0.0+
- **GCC ARM Toolchain**: 12.2+

### 하드웨어:
- **Raspberry Pi 4B** (모든 RAM 용량 지원)
- **MicroSD 카드**: 16GB+ (Class 10 권장)
- **이더넷 케이블**: TSN 기능 테스트용

---

## 🌐 TSN 기능 테스트

빌드 완료 후 다음 TSN 기능들을 테스트할 수 있습니다:

### IEEE 802.1CB (FRER) 
- 프레임 복제 및 제거
- 네트워크 경로 중복화

### IEEE 802.1Qbv (TAS)
- 시간 인식 스케줄링  
- 결정적 패킷 전송

### IEEE 802.1AS (gPTP)
- 정밀 시간 동기화
- 네트워크 전체 시간 정렬

---

## 📚 추가 정보

- **TSN 설정 가이드**: `doc/tsn_setup_guide.md`
- **API 문서**: `include/zephyr/net/ieee802_1*.h`
- **예제 코드**: `src/main.c`, `src/tsn_demo.c`

---

## 🐛 버그 리포트

빌드 또는 실행 중 문제가 발생하면:
1. 이 가이드의 문제 해결 섹션을 먼저 확인
2. GitHub Issues에 다음 정보와 함께 리포트:
   - 에러 메시지 전문
   - Python 버전 (`python3 --version`)
   - CMake 버전 (`cmake --version`)
   - West 버전 (`west --version`)

**🎯 이제 에러 없이 빌드됩니다!** 🎉