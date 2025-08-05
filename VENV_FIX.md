# 🐍 가상환경으로 TSN 빌드 에러 해결

## ⚡ 원스텝 해결 (Ubuntu externally-managed-environment)

```bash
# 현재 zephyr 디렉토리에서 실행
./fix_build_venv.sh
```

이 스크립트가 자동으로 처리합니다:

1. ✅ **필요한 시스템 패키지 설치**
   - python3-full, python3-venv, cmake, ninja-build, device-tree-compiler

2. ✅ **Python 가상환경 생성**
   - `tsn_venv` 폴더에 독립된 Python 환경 구성
   - Ubuntu의 externally-managed-environment 정책 우회

3. ✅ **West 및 의존성 설치**
   - west, PyYAML, pyelftools 등 필요한 패키지 설치
   - 시스템 Python과 분리된 안전한 환경

4. ✅ **자동 빌드 실행**
   - West workspace 초기화
   - TSN 샘플 clean 빌드

## 🔄 이후 사용법

한 번 설정 후에는:

```bash
# 가상환경 활성화
source tsn_venv/bin/activate

# 빌드
west build samples/net/tsn/raspberry_pi_4b
```

## 📋 수동 방법 (스크립트가 안 되는 경우)

```bash
# 1. 가상환경 생성
python3 -m venv tsn_venv
source tsn_venv/bin/activate

# 2. 패키지 설치
pip install west PyYAML pyelftools kconfiglib

# 3. West 초기화
west init -l .
west update

# 4. 빌드
west build -p always -b rpi_4b samples/net/tsn/raspberry_pi_4b
```

## 🎯 빌드 성공 후

생성된 바이너리: `build/zephyr/zephyr.bin`

라즈베리파이 4B SD 카드에 플래시:
```bash
sudo dd if=build/zephyr/zephyr.bin of=/dev/sdX bs=4M status=progress
```

🎉 **이제 완전히 작동합니다!**