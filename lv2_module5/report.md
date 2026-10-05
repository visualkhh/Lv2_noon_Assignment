# 최종 보고서 — 비전 객체 추적 시스템

<!-- 주장마다 코드·원본 기록·영상·PR 등 증빙을 링크로 연결합니다.
     수행하지 않은 결과는 채우지 않고 TODO로 둡니다 (PDF: "마지막 확인 지점까지 정직하게").
     문제별 작성 순서: 구현 내용 → 실행 조건 → 결과물 → 측정 결과 → 해석 → 심화 → 한계 -->

> 시험 진행 현황은 [test-checklist.md](test-checklist.md)에서 관리합니다.

## 0. 요약

TODO(팀장): 최종 결과 3~5줄 — 검출률·RMSE·복구 성공률·재현 여부. 측정 후 작성.

## 1. 시스템 개요

### 1.1 목표 대상

- 대상: **파란 사각 기둥** 30×30×60mm (단일 색상 목표 1개)
- 거리: 20cm~1m, 카메라는 pan·tilt 기구로 회전 (전후 이동 없음)
- 사진: TODO(인지) `results/images/target.*`
- 근거: [ros2_ws/src/realsense/config/realsense.yaml](ros2_ws/src/realsense/config/realsense.yaml) 머리말

### 1.2 시스템 구조도

```
RealSense D435 ─/camera/camera/color/image_raw (rgb8, reliable)─▶ perception_node (realsense)
   HSV 마스크 → 잡음 제거 → 컨투어 → 후보 필터 → 대상 선택 → 중심 계산
        │ /target (PointStamped, best-effort, depth 1)   /perception_node/{debug_image,mask}/compressed
        ▼
dynamixel_move_node ── /tracking_status (String, reliable, transient_local)
   상태 머신 IDLE·TRACKING·LOST, 오차 → 위치 변화량 명령
        │ /motor_cmd (JointState, position = Δ[rad], reliable)
        ▼
dynamixel_controller ── USB 시리얼 "M,Δpan,Δtilt\n" [deg], 115200bps
        ▼
OpenCR (firmware/opencr_pan_tilt) ── 목표각 누적 + 범위 제한 → XM430 위치 모드 (ID 11 pan, 12 tilt)
        ▼
카메라 방향 변화 → 새 영상의 오차
```

실행 묶음: `ros2 launch bringup bringup.launch.py` (realsense.launch.py + dynamixel.launch.py)

### 1.3 환경·장비 사양

| 항목 | 값 | 근거 |
|---|---|---|
| 운용 장비 | Raspberry Pi 4 · Ubuntu 26.04 | [results/perception_env_record.md](results/perception_env_record.md) |
| ROS 2 | **Lyrical** (PDF는 Humble로 표기 — Humble은 Ubuntu 22.04 전용이라 26.04에서 Lyrical 사용) | TODO(통합): Pi에서 `printenv ROS_DISTRO` 기록 |
| OpenCV | TODO(인지): Pi 버전 기록 (개발 PC 4.6.0) | perception_env_record.md |
| 카메라 | RealSense D435 (IMU 없음), 컬러 640×480 rgb8, 설정 30fps, 실측 29.95fps (개발 PC) | perception_env_record.md |
| 카메라 내부 파라미터 | fx=607.32, fy=607.19, cx=328.45, cy=256.01 | perception_env_record.md |
| 제어 보드 | OpenCR, Dynamixel2Arduino | [firmware/opencr_pan_tilt](firmware/opencr_pan_tilt) |
| 모터 | XM430-W350-T × 2 (ID 11 pan, 12 tilt), Protocol 2.0, 1Mbps | [config.h](firmware/opencr_pan_tilt/config.h) — TODO(제어): 실기 확인 표시 |
| 회전 범위 | 목표각 −180° ~ 179.9° (모터 180° 중심 기준) | config.h — TODO(제어): 실제 기구 간섭 범위로 좁히기 |
| 제어 통신 | USB 시리얼 `/dev/ttyACM0`, 115200bps | [dynamixel.yaml](ros2_ws/src/dynamixel/config/dynamixel.yaml) |

### 1.4 시험 계획 (사전 확정 조건)

TODO(검증): 결과를 보기 전에 확정 — 대상 위치 표시, 거리, 조명, 해상도, Kp 두 값, 반복 순서.
기준: [test-checklist.md](test-checklist.md)

## 2. 문제 1 — HSV·Contour 검출 파이프라인

### 2.1 구현 내용

- 패키지: [ros2_ws/src/realsense](ros2_ws/src/realsense) (`realsense::PerceptionNode`, C++)
- 처리 순서: rgb8 → BGR → HSV 마스크(범위 1 OR 범위 2) → open/close → `findContours(RETR_EXTERNAL)` → 면적비·종횡비·채움비 필터 → **통과한 후보 중 면적 최대 1개** → 모멘트 무게중심
- 표시: 영상 중심 십자, 선택 대상 박스·윤곽·중심, 탈락 후보(빨강 + 사유)
- 미검출이면 이전 좌표를 재사용하지 않고 z = 0 발행
- 정규화: ex = (cx − W/2)/(W/2), ey = (cy − H/2)/(H/2), 오른쪽·아래 +, 면적비 = contour_area/(W×H)
- 상세: [ros2_ws/src/realsense/README.md](ros2_ws/src/realsense/README.md)

### 2.2 설정

[realsense.yaml](ros2_ws/src/realsense/config/realsense.yaml)

| 파라미터 | 값 | 근거 |
|---|---|---|
| `hsv_lower` / `hsv_upper` (범위 1) | [103, 220, 20] / [130, 255, 255] | V 하한 20: 어두운 역광의 기둥 V 37~42 (`test/data/dim_backlit_20261002.png`). H 하한 103·S 하한 220: 하늘색 판(H 중앙값 98) 제외 (`panel_and_pillar_20261002.png`) |
| `hsv_bright_lower` / `upper` (범위 2) | [103, 150, 130] / [130, 255, 255] | 조명 아래 기둥은 S가 170~202로 내려가 범위 1만으로는 7~12%만 남아 미검출 → 밝은 픽셀(V ≥ 130)만 S 150까지 허용 (`lit_pillar_20261002.png`) |
| `min_area_ratio` | 0.0006 (640×480에서 약 184px) | 1m에서 기둥 ≈ 650px, 일부 가려지면 절반 |
| `max_area_ratio` | 0.15 | 20cm 정면 0.054, 45° 회전 0.075의 약 2배 |
| `open_kernel` / `close_kernel` | 3 / 5 | 작은 점 제거 / 작은 구멍 메우기 |
| `aspect_max` / `fill_min` | 4.0 / 0.6 | 세운 기둥 ≈ 2.0, 45° ≈ 1.4 |
| 해상도 | 640×480 @ 30fps | `realsense.launch.py color_profile` |
| 영상 구독 QoS | reliable (`image_reliable: true`) | best-effort 1~2fps vs reliable 30fps (perception_env_record.md, 2026-10-02) |

### 2.3 장면별 결과 (정상·없음·가림)

| 장면 | 원본 | 마스크 | 검출 | 결과 |
|---|---|---|---|---|
| 정상 | TODO(인지) | TODO | TODO | TODO |
| 없음 | TODO | TODO | TODO | TODO |
| 일부 가림 | TODO | TODO | TODO | TODO |

참고(개발 PC 실측 장면): `test/data/{dim_backlit,lit_pillar,panel_and_pillar}_20261002.png` — gtest `test_detector`로 검출 규칙을 회귀 시험 중.

### 2.4 분석
- HSV 범위 근거: 2.2 표 + [perception_env_record.md](results/perception_env_record.md) "조명 조건별 검출 확인"
- 배경 오검출 감소 조건: H 하한 103(하늘색 판 제외), S 하한, 면적비 상·하한, 종횡비·채움비
- 미검출 전달 방식: 정상 영상에서 미검출이면 z = 0을 매 프레임 발행 (x·y는 제어에 쓰지 않음)

### 2.5 심화 (선택)

조명 변화(역광 V 하한 50 → 20)에 따른 검출 비교가 개발 PC에서 진행됨 — 검출률·FPS 정식 비교는 TODO(인지)

## 3. 문제 2 — 인지·제어 노드 연결

### 3.1 노드·연결 구조도

1.2 구조도 참고. 실기 묶음 `bringup`, 테스트베드 묶음 `fake_camera_bringup`(8.4 참고).

### 3.2 인터페이스 표

| 항목 | 규약 | 구현 위치 |
|---|---|---|
| 목표 토픽 | `/target` · `geometry_msgs/msg/PointStamped` | perception_node |
| point.x / point.y | 정규화 중심 오차 ex / ey (오른쪽·아래 +, −1~+1) | `detector.cpp` |
| point.z | 면적비, **0 = 미검출** (이때 x·y로 제어하지 않음) | perception_node, dynamixel_move_node |
| header.stamp | 원본 영상 시각 유지 (새 시각을 붙이지 않음) | `perception_node.cpp` `target.header = msg->header` |
| 발행 | 영상 처리마다 발행, 정상 영상의 미검출은 z = 0 | perception_node |
| QoS | `/target` best-effort · volatile · depth 1 / 영상 구독 reliable (2.2 근거) | 양쪽 노드 |
| 상태 | `/tracking_status` · `std_msgs/String` · IDLE/TRACKING/LOST · reliable + transient_local · **상태가 바뀔 때만 발행** | `dynamixel_move_node.cpp` |
| 모터 명령 (ROS) | `/motor_cmd` · `sensor_msgs/JointState` · name `[pan_joint, tilt_joint]` · **position = 이번 변화량 [rad]** · velocity/effort 비움 · /target 수신마다(오차가 데드밴드 밖일 때만) | `dynamixel_move_node.cpp` |
| 모터 명령 (시리얼) | `M,<Δpan>,<Δtilt>\n` [deg, 소수 4자리], 115200bps | `dynamixel_controller.cpp` |
| 펌웨어 동작 | 목표각 += Δ, −180~179.9° 제한, 위치 모드로 이동 (모터 180° 중심 기준) | `opencr_pan_tilt.ino` |
| 정지 명령 | 별도 정지 명령 없음 — 변화량을 보내지 않으면 마지막 목표각에서 정지 (TODO(제어): 실제 정지 확인 기록, PDF 문제 4 "위치형은 갱신 중단만으로 정지한다고 가정하지 말 것") | — |
| 타임아웃 | 마지막 유효 `/target` 후 0.5초 → LOST (`lost_timeout`) | dynamixel_move_node |

TODO(통합): 상태 토픽 주기 결정 — 변경 시만(현재) vs 변경 시 + 1Hz. 결정과 이유를 여기 기록.

### 3.3 입력별 확인 결과 (모터 출력 OFF)

`ros2 launch dynamixel dynamixel.launch.py use_motor:=false` + `/target` 직접 발행.

| 입력 | 기대 결과 | 명령 | 상태 | 로그 |
|---|---|---|---|---|
| x=0, z>0 | 중심에서 불필요한 회전 없음 | TODO | TODO | TODO |
| x=+0.4, z>0 | 오른쪽 오차를 줄이는 명령 | TODO | TODO | TODO |
| x=−0.4, z>0 | 반대 방향 명령 | TODO | TODO | TODO |
| z=0 | 이전 목표를 쫓지 않고 정지 | TODO | TODO | TODO |
| 발행 중단 | 0.5초 뒤 타임아웃 정지 | TODO | TODO | TODO |

TODO(통합): 테스트베드에서 위 5개 입력을 자동으로 넣고 로그를 남기는 스크립트

### 3.4 분석
- 부호가 반대일 때의 현상: TODO — 오차를 키우는 방향으로 돌아 목표가 화면 밖으로 밀려남 (시험으로 확인 후 기록)
- 미검출과 토픽 침묵의 차이: z = 0은 "영상은 정상, 목표 없음" (즉시 명령 없음), 발행 중단은 "입력 자체가 끊김" (0.5초 타임아웃으로 LOST). TODO: 두 경우 로그 비교
- 노드별 책임: perception = 검출과 오차 계산만 / move_node = 상태·제어 법칙·제한 / controller = 단위 변환·시리얼 전송 / 펌웨어 = 범위 제한·모터 구동

### 3.5 심화 (선택)

`/search` 액션: 미수행

## 4. 문제 3 — 객체 중심 기반 추적 제어

### 4.1 방향·부호 확인

- 설계 부호: 목표가 오른쪽(ex > 0) → `pan_gain` −0.03 → pan 변화량 음수 → 오른쪽으로 회전 (URDF·시뮬레이션에서 확인)
- TODO(제어): 실기에서 추적을 끈 상태로 작은 명령 → 실제 영상 오차가 줄어드는지 확인, 영상·로그

### 4.2 제어식과 제한값

현재 제어식 (위치 변화량형, `/target` 1개마다):
`Δpan = clamp(pan_gain × ex, ±max_pan_command)` (|ex| ≤ deadband이면 0), tilt도 같은 형태

| 항목 | 값 | 단위 |
|---|---|---|
| `pan_gain` | −0.03 | rad / (정규화 오차 · 프레임) |
| `tilt_gain` | 0.06 | rad / (정규화 오차 · 프레임) |
| `max_pan_command` / `max_tilt_command` | 0.0872665 (= 5°) | rad / 프레임 |
| `horizontal_deadband` / `vertical_deadband` | 0.05 | 정규화 오차 |
| `lost_timeout` | 0.5 | s |
| 펌웨어 목표각 범위 | −180 ~ 179.9 | deg |

> ⚠️ TODO(제어): 변화량이 **프레임당** 값이라 실제 회전 속도가 FPS에 비례합니다 (30fps면 10fps의 3배).
> PDF 문제 3-4 "속도를 위치로 적분한다면 실제 경과 시간 dt 사용" → `Δ = gain × ex × dt` (gain 단위 rad/s)로 바꾸거나, FPS를 고정·기록하고 이 한계를 적을 것.
> 수평 1축이 필수 범위이므로 필수 시험은 tilt를 끈 설정(예: `tilt_gain: 0`)으로 하는 것을 권장.

### 4.3 시험 조건

TODO(검증): 왼쪽 3초 → 중앙 3초 → 오른쪽 3초 → 중앙 3초, 위치 표시, 거리·해상도 고정, Kp별 3회

### 4.4 Kp 비교 결과

| Kp | 회차 | 지표 | CSV | 그래프 |
|---|---|---|---|---|
| TODO (A) | 1~3 | TODO | TODO | TODO |
| TODO (B) | 1~3 | TODO | TODO | TODO |

### 4.5 분석
- 최종 설정 선택 근거: TODO
- 반응 속도·흔들림 차이: TODO
- 모터 위치를 측정하지 않았다면 명령을 실제 위치처럼 표시하지 않음 (그래프 축 = 명령)

### 4.6 심화 (선택)

미수행

## 5. 문제 4 — 성능 측정과 목표 소실 복구

### 5.1 상태 전이 (IDLE · TRACKING · LOST)

[dynamixel_move_node.cpp](ros2_ws/src/dynamixel/src/dynamixel_move_node.cpp)

| 상태 | 조건 | 동작 |
|---|---|---|
| IDLE | 시작 후 아직 유효 목표 없음 | 명령 없음 |
| TRACKING | 유효 `/target`(z > 0, \|x\|·\|y\| ≤ 1) 수신 | 데드밴드 밖이면 제한된 변화량 명령 |
| LOST | 마지막 유효 목표 후 `lost_timeout`(0.5s) 경과 | 명령 없음 (위치 모드라 마지막 목표각 유지) |
| TRACKING 복귀 | 현재: 유효 목표 **1프레임**이면 즉시 복귀 | 제한된 명령으로 재개 |

> ⚠️ TODO(제어): PDF 기본값은 "신선한 목표 **연속 3프레임** 검출 시 복귀"입니다. 현재 구현은 1프레임 — 3프레임으로 바꾸거나 다르게 한 이유를 기록.
> z = 0 프레임에서는 명령을 보내지 않지만 상태 전이는 타임아웃(0.5s) 기준.

### 5.2 통신 타임아웃 (Raspberry Pi · OpenCR)

- Raspberry Pi 측: `/target` 0.5초 타임아웃 → LOST (구현됨)
- OpenCR 측: ⚠️ **미구현** — 현재 펌웨어 `loop()`는 시리얼 수신만 함. TODO(제어): 명령 미수신 N ms 후 정지(현재 위치를 목표로 고정 등) 구현 + 시험 (PDF p3·문제 4-4 필수)

### 5.3 시험 결과

| 시험 | 횟수·조건 | 결과 | 기록 |
|---|---|---|---|
| 정상 추적 | 30초 이상 | TODO | TODO |
| 가림 후 재등장 | 약 2초 가림, 5회 | TODO | TODO |
| 인지 입력 중단 | `/target` 발행 중단 1회 이상 | TODO | TODO |
| 제어 통신 중단 | 제어 프로그램 종료 등 1회 이상 | TODO | TODO |

### 5.4 성능표

| 지표 | 산식 | 값 | 기록 조건 |
|---|---|---|---|
| 처리 FPS | 처리 완료 프레임 수 / 실제 경과 초 | TODO | 카메라 설정 FPS(30)와 구분 |
| 검출률 | 올바른 검출 / 목표가 보이는 평가 프레임 × 100 | TODO | 최소 30프레임 사람 대조, 목록 저장 |
| 배경 오검출 | 목표 없는 평가 프레임의 잘못된 검출 수 | TODO | 최소 10프레임 |
| 수평 RMSE | sqrt(mean(ex²)) | TODO | 검출·TRACKING 구간, 제외 수·추적 비율 병기 |
| 복구 성공률 | 3초 이내 TRACKING 복귀 / 5 × 100 | TODO | |
| 복구 시간 | TRACKING 복귀 시각 − 재등장 시각 | TODO | 실패는 실패로 표시 |

### 5.5 복구 시험 (5회)

| 회차 | 정지 여부 | 복귀 여부 | 복구 시간 | 비고 |
|---|---|---|---|---|
| 1 | TODO | TODO | TODO | |
| 2 | TODO | TODO | TODO | |
| 3 | TODO | TODO | TODO | |
| 4 | TODO | TODO | TODO | |
| 5 | TODO | TODO | TODO | |

### 5.6 실패 원인 분석

TODO: 검출 · 통신 · 제어 중 어디인지 근거와 함께

### 5.7 심화 (선택)

SEARCHING: 미수행

## 6. 문제 5 — bag 재현과 팀 협업

### 6.1 bag 메타데이터

[recordings/README.md](recordings/README.md) — TODO(통합)

### 6.2 입력 재처리 결과

`ros2 launch realsense realsense.launch.py use_camera:=false target_topic:=/target_replay` + `ros2 bag play` (모터 출력 끔) — TODO(통합): 실행·결과 이미지

### 6.3 결과 재분석 결과

TODO: 저장된 `/target`·상태·명령으로 5.4 지표 재계산

### 6.4 원본 vs 재현 비교

TODO

### 6.5 다른 팀원 실행 기록

| 확인자 | 날짜 | 기준 커밋 | 실행 내용 | 결과 | 수정한 누락 사항 |
|---|---|---|---|---|---|
| TODO | | | | | |

### 6.6 심화 (선택)

미수행

## 7. 도전 실습

| 항목 | 수행 여부 | 담당 | 결과 |
|---|---|---|---|
| A — 환경 변화에 강한 검출 | 진행 중 (역광 조건 V 하한 비교, 개발 PC) | 인지 | TODO |
| B — 인터페이스 완성도와 SEARCHING | 미수행 | | |
| C — 추적 제어 성능 비교 | 미수행 | | |
| D — 목표 소실 복구 강건성 | 미수행 | | |
| E — 고정 bag 회귀 비교 | 미수행 | | |
| (선택 범위) 2축 추적 | 구현됨 (pan + tilt) — 시험 TODO | 제어 | |

## 8. 한계와 개선 방향

### 8.1 수행하지 않은 기능·측정하지 않은 결과

TODO: 측정 후 정리. 현재 확인된 미구현: OpenCR 통신 타임아웃, dt 기반 제어, 3프레임 복귀 조건, CSV 기록, bag 도구

### 8.2 개선 방향

TODO

### 8.3 외부 코드·AI 도구 사용 범위

PDF: "외부 코드·모델·AI 도구의 사용 범위와 직접 수정·검증한 부분을 구분"

| 범위 | 도구·출처 | 팀이 직접 수정·검증한 부분 |
|---|---|---|
| 테스트베드 (`docker/`, CI, `test-*` 스크립트) | AI 코딩 도구(Claude Code)로 작성 | TODO(팀장) |
| `bringup`, `fake_camera_bringup` 패키지 (fake_camera, monitor_manager) | AI 코딩 도구 | TODO |
| 통제실·3D 시뮬레이션 (`docker/test-controller/`: run-controller.py, scene.py, pan_tilt.urdf) | AI 코딩 도구, pytransform3d, Poly Haven 파노라마(CC0) | TODO |
| `firmware/upload.sh` | AI 코딩 도구, ROBOTIS opencr_update(opencr_ld_shell) | TODO |
| 인지 (`realsense`) | TODO(인지) | |
| 제어 (`dynamixel`, 펌웨어) | TODO(제어) | |

### 8.4 대체 환경 (테스트베드·시뮬레이션)

- Docker 테스트베드: Ubuntu 26.04 + ROS 2 Lyrical, 가상 시리얼(`/dev/ttyACM0` → PTY → `debug/serial-out`)
- 3D 시뮬레이션: 기구 URDF(치수 추정값) + 360° 배경 + 가상 기둥·장애물, 관절각 = 시리얼 명령 누적(펌웨어와 같은 계산, 모터 지연은 모델링 안 함)
- 용도: 인터페이스 검증, 시험 리허설. **시뮬레이션 결과는 실기 결과로 보고하지 않음**
- 실행: [README.md](README.md)

## 9. 필수 요구사항 추적표

| 요구사항 | 구현 | 증빙 | 보고서 위치 |
|---|---|---|---|
| HSV·Contour 검출, 설정 파일 분리 | ✅ realsense | realsense.yaml, test_detector | 2 |
| 정상·없음·가림 장면 확인 | ☐ | TODO | 2.3 |
| `/target` 인터페이스 (x·y·z·stamp·QoS) | ✅ | perception_node.cpp | 3.2 |
| 모터 OFF 5개 입력 확인 | ☐ | TODO | 3.3 |
| 수평 1축 P 추적 | ✅ (pan) | dynamixel_move_node.cpp | 4 |
| dt 기반 적분 | ⚠️ 미반영 | — | 4.2 |
| Kp 2종 × 3회 비교 | ☐ | TODO | 4.4 |
| IDLE·TRACKING·LOST | ✅ | dynamixel_move_node.cpp | 5.1 |
| 3프레임 복귀 | ⚠️ 1프레임 | — | 5.1 |
| Pi 측 입력 타임아웃 | ✅ 0.5s | dynamixel.yaml | 5.2 |
| OpenCR 측 통신 타임아웃 | ⚠️ 미구현 | — | 5.2 |
| 30초·가림 5회·중단 시험, 지표 | ☐ | TODO | 5.3~5.5 |
| bag 기록·재처리·재분석 | ☐ | TODO | 6 |
| 다른 팀원 재현 | ☐ | TODO | 6.5 |
| 4인 PR·리뷰 | ◐ PR #1, #4 병합 | team.md | — |
