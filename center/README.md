center (웹 통제실)
=====

`run-controller.py` (tkinter) 대신 브라우저에서 여는 통제실. 프레임워크는
repo 루트 submodule `packages/` (`dooboostore-develop/packages` repo)의 TS 원본을 쓴다.
npm 발행판은 소스와 어긋나므로 쓰지 않음.

```
Lv2_noon_Assignment/
├── packages/         # submodule → git@github.com:dooboostore-develop/packages.git
└── center/
    ├── src/              # 공용 (bootfactory·AppBody·라우터·페이지·ControlService 타입)
    ├── front-end/        # 브라우저 진입 (index.ts·index.html·ControlFrontService)
    ├── back-end/         # 서버 (index.ts·ControlBackService·environment)
    └── dist-front-end/ · dist-back-end/  # 빌드 결과 (git 추적 안 함)
```

- debug 폴더 1개 = target 1개(= 1 ROS_DOMAIN_ID). 여러 폴더는 `CENTER_DEBUG_ROOTS`
  (또는 `--debug-roots=`) 콜론 구분으로 늘어나면 target 선택으로 바뀜.
- 백엔드는 파일만 읽고 ROS는 몰라도 됨 (컨테이너와 공유 폴더로 주고받음 — 기존 통제실과 같은 방식).
- 닫힌 루프: World 페이지 three.js 렌더 → `putFrame` → `input-live/frame.png` → fake_camera → perception → `/target` → dynamixel → serial → 관절각 → 다시 렌더.

## 실행

```bash
# 최초 1회: submodule + 의존성 (pnpm — npm은 entities 버전 충돌로 백엔드 빌드 깨짐)
git submodule update --init packages
cd center
pnpm install

# 빌드 + 실행 (기본 debug 루트 = ../lv2_module5/debug, 포트 3032)
pnpm run build
pnpm start

# 다른 debug 폴더 / 포트
pnpm start -- --debug-roots=/path/a/debug:/path/b/debug --port=3032
# 또는 CENTER_DEBUG_ROOTS=/path/a/debug:/path/b/debug PORT=3032 pnpm start
```

페이지: `/` 개요(status·motors·images·serial, 1초 폴링) · `/world` 3D(기둥 드래그·송출 토글).

## 상태

- 1차: 껍데기+모니터 읽기+serial-in 쓰기+송출 끄기+three.js 기본 장면+putFrame 송출.
- 후속: URDF 기구 형상·360° 배경·HSV 범위 판정·장애물 추가/삭제·월드 시점 회전 이식 (기준: `docker/test-controller/scene.py`).
