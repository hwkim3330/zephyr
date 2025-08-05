# 🚀 TSN 빌드 에러 즉시 해결

## ⚡ 원스텝 해결방법

사용자가 보고한 에러를 즉시 해결하는 방법:

```bash
# 현재 디렉토리에서 실행 (zephyr 폴더 안에서)
./fix_build.sh
```

이 스크립트가 자동으로 다음을 처리합니다:
- ✅ 스크립트 권한 수정
- ✅ Python 환경 Miniconda → 시스템 Python 변경  
- ✅ West 재초기화
- ✅ 의존성 설치
- ✅ Clean 빌드 실행

## 🛠️ 수동 해결 (위 스크립트가 안 되는 경우)

### 1단계: 환경 변수 설정
```bash
export PYTHON_EXECUTABLE=/usr/bin/python3
export WEST_PYTHON=/usr/bin/python3
export PATH="/usr/bin:/bin:/usr/local/bin:$PATH"
```

### 2단계: West 재초기화
```bash
rm -rf .west build
west init -l .
west update
```

### 3단계: 빌드
```bash
west build -p always -b rpi_4b samples/net/tsn/raspberry_pi_4b
```

## 🔍 에러 원인 분석

1. **Permission denied**: 스크립트 실행 권한 없음
2. **Miniconda Python**: CMake가 잘못된 Python 사용
3. **DTS processing error**: 환경 변수 충돌

모두 해결되었습니다! 🎉