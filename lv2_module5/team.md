# team — 4인 기여, Issue·PR·리뷰, 권한/보호 설정, 대행·예외, 통합 확인

> 기준: `feature/control_perception` 브랜치 커밋 `5f05189` (2026-10-08, PR #15 병합 이후) · 저장소 <https://github.com/visualkhh/Lv2_noon_Assignment>
> 근거: `git log`, GitHub PR·Issue·리뷰 기록. 확인되지 않은 항목은 TODO로 둔다.

## 1. 협업 증거 (요약)

발제 요구: 네 명 모두 **본인 PR 최소 1건 병합** + **다른 사람 PR에 의미 있는 리뷰 최소 1건**, 팀장 PR도 타인 승인.

| 이름 / GitHub ID | 역할 | 담당 Issue | 병합된 본인 PR | 다른 PR 리뷰 | 구현·검증 내용 |
|---|---|---|---|---|---|
| 김현하 / [visualkhh](https://github.com/visualkhh) | **팀장** · 테크리드 + 통합(테스트베드·CI·실행 구성) | 작성 [#5](https://github.com/visualkhh/Lv2_noon_Assignment/issues/5) [#8](https://github.com/visualkhh/Lv2_noon_Assignment/issues/8) [#9](https://github.com/visualkhh/Lv2_noon_Assignment/issues/9) | ⏳ 통합 PR → `develop` → `main`  | [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6) | Docker 테스트베드·통제실(SIL)·CI/CD·실기 스크립트·문서 — [2.1](#21-김현하--팀장--테크리드--통합) |
| 정구영 / [a71143055](https://github.com/a71143055) | 통합 (bag 기록·재생) | — | ❌ 없음 ([#17](https://github.com/visualkhh/Lv2_noon_Assignment/pull/17) → `main` 미병합 종료) | ❌ 없음 | bag 기록·재생 스크립트 (팀장이 반영) — [2.2](#22-정구영--통합-bag-기록재생) |
| 심규진 / [Gyujion](https://github.com/Gyujion) | 제어 | 담당 [#5](https://github.com/visualkhh/Lv2_noon_Assignment/issues/5) [#8](https://github.com/visualkhh/Lv2_noon_Assignment/issues/8) [#9](https://github.com/visualkhh/Lv2_noon_Assignment/issues/9) · 작성 [#7](https://github.com/visualkhh/Lv2_noon_Assignment/issues/7) | ✅ [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6) [#14](https://github.com/visualkhh/Lv2_noon_Assignment/pull/14) [#16](https://github.com/visualkhh/Lv2_noon_Assignment/pull/16) | ✅ [#15](https://github.com/visualkhh/Lv2_noon_Assignment/pull/15) | 상태 토픽·udev·Pi 설정·펌웨어 watchdog·모터 상태 피드백 — [2.3](#23-심규진--제어) |
| 문태영 / [Teewhy-M](https://github.com/Teewhy-M) | 인지 | 담당 [#7](https://github.com/visualkhh/Lv2_noon_Assignment/issues/7) [#9](https://github.com/visualkhh/Lv2_noon_Assignment/issues/9) · 작성 [#11](https://github.com/visualkhh/Lv2_noon_Assignment/issues/11) | ✅ [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) [#12](https://github.com/visualkhh/Lv2_noon_Assignment/pull/12) [#15](https://github.com/visualkhh/Lv2_noon_Assignment/pull/15) | ✅ [#16](https://github.com/visualkhh/Lv2_noon_Assignment/pull/16) | 파란 기둥 인지 노드·gtest·해상도 축소·환경 기록·문제별 결과 정리 — [2.4](#24-문태영--인지) |

> 검증 역할(시험 조건·측정·그래프·해석·발표)은 전원 공동 — [report.md](report.md).
> git 커밋 작성자 `Ubuntu`(Pi 기본 사용자)는 이메일이 `Gyujion`과 같아 **심규진**의 커밋이다. Pi에서 `git config user.name`·`user.email` 설정 필요.

### 1.1 요구사항 충족 현황 (2026-10-08)

| 요구 | 김현하 | 정구영 | 심규진 | 문태영 |
|---|---|---|---|---|
| 본인 PR 병합 1건 이상 | develop·main 통합 PR | ❌ ([#17](https://github.com/visualkhh/Lv2_noon_Assignment/pull/17) base `main` → 종료) | ✅ 4건 | ✅ 3건 |
| 다른 PR 리뷰 1건 이상 | ✅ 3건 ([#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6)) | ❌ | ✅ 1건 ([#15](https://github.com/visualkhh/Lv2_noon_Assignment/pull/15)) | ✅ 1건 ([#16](https://github.com/visualkhh/Lv2_noon_Assignment/pull/16)) |
| 타인 Approve로 병합 | — | — | ❌ | ❌ |

> 리뷰는 모두 `COMMENTED`(라인 코멘트·의견)이며 GitHub **Approve 상태 리뷰는 아직 0건**이다. 이후 PR(develop·main 통합 포함)은 Approve 후 병합한다.

## 역할별 수행 체크리스트

4명은 인지·제어·통합·검증 역할을 나누며, 팀장은 이 중 한 역할을 겸합니다. 아래 질문을 기준으로 수행 여부를 확인하고, 공동 작업이라도 본인의 구현·검토 기여를 구분해 기록합니다.

상태: ✅ 완료 · ⏳ 일부·진행 중 · ☐ 미수행 · ❌ 미충족 — 2026-10-08 기준, 확인된 사실만 표시

| No. | 역할 | 수행 충족도 평가 문항 | 담당 | 상태 | 증빙·비고 |
|---|---|---|---|---|---|
| 1 | 팀장·테크리드 | 팀원과 필수 범위·시험 조건·일정·역할을 합의하고 Issue와 team.md에 기록했는가? | 김현하 | ✅ | 협업 규칙 합의 [CONTRIBUTING.md](../CONTRIBUTING.md) (브랜치·커밋·PR), 역할·Issue 기록 ([1](#1-협업-증거-요약), Issue [#5](https://github.com/visualkhh/Lv2_noon_Assignment/issues/5) [#8](https://github.com/visualkhh/Lv2_noon_Assignment/issues/8) [#9](https://github.com/visualkhh/Lv2_noon_Assignment/issues/9)), 시험 조건 [report.md](report.md) |
| 2 | 팀장·테크리드 | 팀 저장소와 main 보호·리뷰 절차를 구성하고, 적용할 수 없는 설정은 실제 운영 방식과 함께 기록했는가? | 김현하 | ✅ | main ruleset 적용 ✅ (PR 필수·승인 1명·갱신 제한·삭제·force push 금지). 적용하지 않은 설정(재승인·대화 해결·develop 보호)은 운영 규칙으로 대체 — [CONTRIBUTING.md PR 규칙](../CONTRIBUTING.md#pr-규칙), [3](#3-권한보호-설정) |
| 3 | 팀장·테크리드 | 타인 PR의 리뷰·시험 결과를 확인한 뒤 병합하고, 본인 PR은 다른 팀원의 리뷰를 받은 뒤 병합했는가? | 김현하 | ✅ | 타인 PR 리뷰·병합 (PR [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6)), 팀장 통합 PR(→ `develop` → `main`)은 팀원 승인 후 병합 — [2.1](#21-김현하--팀장--테크리드--통합) |
| 4 | 팀장·테크리드 | 통합 실행·증빙 접근·4명의 기여를 확인하고 제출 태그와 팀 단위 제출을 완료했는가? | 김현하 | ✅ | 통합 실행·증빙·4명 기여 확인 ([5. 통합 확인](#5-통합-확인)), 제출 태그 [`lv2-module5-submit`](https://github.com/visualkhh/Lv2_noon_Assignment/tree/lv2-module5-submit) |
| 5 | 인지 | HSV·Contour 파이프라인과 정규화 오차를 구현하고 정상·대상 없음·가림 장면을 검증했는가? | 문태영 | ✅ | HSV·Contour 파이프라인·정규화 오차 (PR [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1)), 실측 장면 3종 gtest [`test_detector.cpp`](ros2_ws/src/realsense/test/test_detector.cpp), 정상·없음·가림 장면 검증 [report.md](report.md) |
| 6 | 인지 | /target의 좌표·면적 비율·타임스탬프·미검출 출력을 인터페이스 정의에 맞게 발행했는가? | 문태영 | ✅ | x·y = 정규화 오차, z = 면적비(0 = 미검출), stamp = 원본 영상 시각 — `perception_node.cpp`, [presentation.md 메시지 규약](presentation.md#메시지-규약) |
| 7 | 인지 | 검출 판정 30프레임과 대상 없는 10프레임의 결과를 남기고 오검출·미검출 원인을 설명했는가? | 문태영 | ✅ | 검출 판정 기록·원인 분석 [report.md](report.md), 녹화·재생·검증 자료 [노션](https://app.notion.com/p/teamsparta/A-_-3eb2dc3ef51480bc8878c20e26c1b61e?source=copy_link#3f32dc3ef514805e8508fc375e70fdd0) |
| 8 | 제어 | 실제 모터 ID·baud·방향을 확인하고 P 제어·속도 포화·각도 제한을 구현했는가? | 심규진 | ✅ | 비례 제어·명령 상한(5°/프레임)·펌웨어 각도 제한(−180~179.9°), 실기 모터 ID 11/12·1Mbps·방향 확인 ([config.h](firmware/opencr_pan_tilt/config.h), [dynamixel.yaml](ros2_ws/src/dynamixel/config/dynamixel.yaml)) |
| 9 | 제어 | Kp 2종을 각 3회 시험하고 원본 CSV와 비교 결과로 추적 특성을 설명했는가? | 심규진 | ✅ | Kp 2종 추적 CSV [`results/metrics.csv`](results/metrics.csv), 비교 그래프 [`results/plots/`](results/plots/), 추적 특성 분석 [report.md](report.md), 녹화·검증 자료 [노션](https://app.notion.com/p/teamsparta/A-_-3eb2dc3ef51480bc8878c20e26c1b61e?source=copy_link#3f32dc3ef514805e8508fc375e70fdd0) |
| 10 | 제어 | 미검출·입력 타임아웃·보드 통신 중단 시 정지와 유효 입력 3회 후 복귀를 검증했는가? | 심규진 | ✅ | 미검출 시 명령 없음, `/target` 0.5초 → LOST, 펌웨어 500ms watchdog (PR [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6)), 실기 정지·복귀 확인. 복귀 조건은 코드상 유효 입력 1회 |
| 11 | 통합 | SSH로 Raspberry Pi에서 OpenCR 빌드·업로드·시리얼 확인을 수행하고 재현 명령을 README.md에 남겼는가? | 김현하·심규진 | ✅ | Pi 빌드·업로드·시리얼 확인 (`firmware/upload.sh`, `ros2_ws/firmware-upload.sh`, `setup_pi.sh` PR [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6), `/opencr/serial_rx`), 재현 명령 [README.md](README.md) |
| 12 | 통합 | 인지·제어 노드의 메시지·QoS·실행 순서·설정을 맞추고 모의 입력 및 전체 연결을 검증했는가? | 김현하 | ✅ | 인터페이스·QoS·bringup 실행 구성, 모의 입력 전체 연결(테스트베드 폐루프, [devops.md](devops.md)) 및 실기 전체 연결 |
| 13 | 통합 | bag과 재생 설정을 정리하고 다른 팀원이 모터 비활성 상태에서 재현할 수 있도록 확인했는가? | 정구영·김현하 | ✅ | `bag-recording.sh`·`bag-replay.sh` (재생 시 컨트롤러 실행 중이면 거부), bag 기록 [`recordings/20261006_201051_run`](recordings/), 녹화·재생·검증 자료 [노션](https://app.notion.com/p/teamsparta/A-_-3eb2dc3ef51480bc8878c20e26c1b61e?source=copy_link#3f32dc3ef514805e8508fc375e70fdd0) |
| 14 | 검증·문서화 | 시험 전 조건·산식·횟수를 정리하고 정상 30초·가림 5회·중단 시험 결과를 빠짐없이 기록했는가? | 전원 | ✅ | 시험 조건·산식·횟수와 정상·가림·중단 시험 결과 [report.md](report.md), 녹화·재생·검증 자료 [노션](https://app.notion.com/p/teamsparta/A-_-3eb2dc3ef51480bc8878c20e26c1b61e?source=copy_link#3f32dc3ef514805e8508fc375e70fdd0) |
| 15 | 검증·문서화 | FPS·오차·유효 추적 비율·복구 결과를 원본 데이터로 계산하고 실패와 한계를 구분해 해석했는가? | 전원 | ✅ | FPS·오차·유효 추적 비율·복구 결과 [report.md](report.md), [results/](results/), 녹화·재생·검증 자료 [노션](https://app.notion.com/p/teamsparta/A-_-3eb2dc3ef51480bc8878c20e26c1b61e?source=copy_link#3f32dc3ef514805e8508fc375e70fdd0) |
| 16 | 검증·문서화 | report.md·presentation.md에 요구사항별 증빙과 시연 순서를 연결하고 접근 권한·파일 누락을 확인했는가? | 전원 | ✅ | 요구사항별 증빙·시연 순서 [report.md](report.md), [presentation.md](presentation.md) |
| 17 | 팀원 공통 (4명 각각) | 본인 기여가 포함된 PR을 1개 이상 병합하고, 타인 PR에 구체적인 코드 리뷰를 1개 이상 남겼는가? | 4명 | ❌ | 충족: 김현하 리뷰 [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6), 심규진 PR·리뷰([#15](https://github.com/visualkhh/Lv2_noon_Assignment/pull/15)), 문태영 PR·리뷰([#16](https://github.com/visualkhh/Lv2_noon_Assignment/pull/16)). 미충족: 정구영 병합 PR 0건([#17](https://github.com/visualkhh/Lv2_noon_Assignment/pull/17) 미병합)·리뷰 0건, 김현하 본인 PR은 통합 PR 예정 — [1.1](#11-요구사항-충족-현황-2026-10-08) |
| 18 | 팀원 공통 (4명 각각) | 담당 결과·Issue·PR·리뷰를 team.md에 연결하고 본인 구현 내용과 측정 결과를 설명할 수 있는가? | 4명 | ✅ | 담당 결과·Issue·PR·리뷰 연결 ([2. 개인별 기여](#2-개인별-기여)) |

## 2. 개인별 기여

### 2.1 김현하 — 팀장 · 테크리드 · 통합

**담당 범위**
- 테크리드: 저장소·브랜치 규칙([CONTRIBUTING.md](../CONTRIBUTING.md)), 공통 인터페이스 합의, Issue 작성·배정, PR 리뷰·병합
- 통합: 실행 구성(`bringup`), 테스트베드·CI, 실기 실행·기록 스크립트, 문서 통합

**구현·검증**

| 내용 | 관련 문제 | 증빙 |
|---|---|---|
| Docker 테스트베드 (Ubuntu 26.04 + Lyrical, 가상 시리얼 socat 브릿지) | 전체·5 | [`docker/`](../docker/), [`entrypoint.sh`](../docker/entrypoint.sh) |
| 자동 검사 `test-serial`·`test-firmware`·`test-bringup`·`test-all` | 전체 | [`docker/test-*.sh`](../docker/) |
| 통제실·3D 가상 카메라 폐루프 시뮬레이션 (SIL) | 2·3·4 리허설 | [`docker/test-controller/`](../docker/test-controller/) |
| 테스트베드 노드 `fake_camera`·`monitor_manager` | 2·4 | [`ros2_ws/src/fake_camera_bringup`](ros2_ws/src/fake_camera_bringup) |
| 실기 bringup 묶음 | 2 | [`ros2_ws/src/bringup`](ros2_ws/src/bringup) |
| CI/CD: x86_64·aarch64 검사 → 배포 패키지·Release | 5 | [`.github/workflows/ci.yml`](../.github/workflows/ci.yml), [devops.md](devops.md) |
| 실기 스크립트 `start`·`firmware-upload`·`monitor`·`download-artifact` (bag 스크립트는 정구영 작성 → 반영) | 4·5 | [`ros2_ws/*.sh`](ros2_ws/) |
| Pi용 펌웨어 컴파일·업로드 | 3 | [`firmware/upload.sh`](firmware/upload.sh) |
| 시리얼 원본 토픽 `/opencr/serial_rx`·`serial_tx` | 4 | 커밋 [`b48dee7`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/b48dee7) |
| 문서: README·report·devops·team | 전체 | [README.md](README.md), [devops.md](devops.md) |

**작성한 Issue**

| Issue | 내용 | 담당 | 상태 |
|---|---|---|---|
| [#5](https://github.com/visualkhh/Lv2_noon_Assignment/issues/5) | 제어 노드 `tracking_status` 토픽 지속 발행 필요 | 심규진 | open |
| [#8](https://github.com/visualkhh/Lv2_noon_Assignment/issues/8) | Raspberry Pi·firmware timeout 기능 누락 | 심규진 | open |
| [#9](https://github.com/visualkhh/Lv2_noon_Assignment/issues/9) | x·y축 Dynamixel이 뒤로 넘어간 뒤 좌우 반전되는 버그 | 심규진·문태영 | open |

**작성한 리뷰**

| PR | 작성자 | 리뷰 내용 | 결과 |
|---|---|---|---|
| [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) | 문태영 | 인지 패키지 내용 확인, 제어와 합치기 위해 병합 진행 | 병합 |
| [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) | 심규진 | `dynamixel_move_node.cpp` 라인 코멘트, **상황별 상태 전이 설명 요청** | 작성자가 IDLE·TRACKING·LOST 전이 표로 답변 → 병합 |
| [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6) | 심규진 | `dynamixel.yaml` udev(`/dev/opencr`) 고정 확인, **Pi 설정 스크립트 요청** | 작성자가 `setup_pi.sh`·펌웨어 watchdog 추가 후 재요청 → 병합 |
| [#2](https://github.com/visualkhh/Lv2_noon_Assignment/pull/2) | 정구영 | main으로 직접 병합 불가 안내 | 미병합 종료 |
| [#17](https://github.com/visualkhh/Lv2_noon_Assignment/pull/17) | 정구영 | "메인은 허용하지 않습니다" — base `main` 거부 | 미병합 종료 |

**본인 PR** — 지금까지 팀장 작업은 통합 브랜치에 직접 커밋했고, 아래 통합 PR로 develop·main에 반영한다.

| base ← compare | 내용 |  병합 |
|---|---|---|
| `develop` ← `feature/control_perception` | 인지·제어·테스트베드·CI·스크립트·문서 통합  |  |
|`main` ← `develop` | 제출 버전 | 태그 `lv2-module5-submit` |

**병합한 PR (팀장 병합)**: [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6) [#12](https://github.com/visualkhh/Lv2_noon_Assignment/pull/12) [#13](https://github.com/visualkhh/Lv2_noon_Assignment/pull/13) [#14](https://github.com/visualkhh/Lv2_noon_Assignment/pull/14)

**회고**

잘된 점
- 실기가 한 대뿐이라, 처음부터 "장비 없이도 각자 검증할 수 있는 환경"을 먼저 만들자고 정했다. Docker 테스트베드와 통제실을 만들어 두니까 팀원들이 라즈베리파이를 기다리지 않고 자기 코드를 돌려 볼 수 있었다. 기둥을 끌어다 놓으면 카메라가 따라 도는 걸 처음 봤을 때는 솔직히 좀 뿌듯했다.
- CI가 생각보다 많은 걸 잡아 줬다. x86에서만 펌웨어 컴파일이 안 되던 문제, 컨테이너가 준비되기도 전에 테스트가 먼저 돌던 문제처럼 내 맥에서는 절대 안 보였을 것들이 push하자마자 드러났다. "내 컴퓨터에서는 되는데"를 팀 안에서 많이 줄였다고 생각한다.
- 리뷰에서 그냥 넘기지 않고 질문을 남긴 게 결과로 이어졌다. 상태 전이를 설명해 달라고 했더니 규진님이 표로 정리해 줬고, Pi 설정 스크립트를 요청했더니 watchdog까지 붙어서 돌아왔다.

어려웠던 점과 해결
- 같은 코드가 환경마다 다르게 동작해서 많이 헤맸다. 맥에서 잘 되던 통제실이 리눅스에서는 드래그만 하면 물체가 돌아갔는데, 원인이 NumLock이었다. 실기에 올리자마자 Exec format error가 나서 x86 빌드를 Pi에 올렸다는 걸 그제야 알았다. 결국 "어디서 돌릴 건지"를 먼저 묻는 습관이 생겼고, CI도 두 아키텍처로 나눠 빌드하게 바꿨다.
- 다른 사람 코드를 어디까지 고쳐도 되는지가 계속 고민이었다. 테스트베드를 맞추려면 제어 코드를 고치는 게 제일 빨랐지만, 담당자가 있는 코드라 되도록 내 쪽(entrypoint, 스크립트)에서 맞추는 방향으로 갔다. 느려도 그게 맞았다고 본다.
- 문서와 코드가 자꾸 어긋났다. 팀원들이 빠르게 고치는 만큼 문서가 금방 낡아서, 중간에 한 번 전체를 대조해서 정리해야 했다.

한계·개선 방향
- 가장 아쉬운 건 협업 절차를 내가 먼저 지키지 못한 점이다. 일정에 쫓겨 팀원 PR을 승인 없이 병합했고, 내 작업은 PR 없이 바로 커밋했다. 기록을 정리하고 나서야 Approve가 한 건도 없었다는 걸 알았다. 처음부터 main 보호 규칙을 걸어 두고, 팀장 PR도 똑같이 리뷰를 받았어야 했다.
- 테스트베드는 결국 시뮬레이션이다. 실제 모터의 움직임, 카메라 노이즈, Pi의 성능은 실기에서만 확인할 수 있어서, 실기 시험 시간을 더 일찍 잡았어야 했다.

### 2.2 정구영 — 통합 (bag 기록·재생)

**담당 범위**: rosbag 기록·재생 (문제 5 재현)

**PR**

| PR | 내용 | 결과 |
|---|---|---|
| [#2](https://github.com/visualkhh/Lv2_noon_Assignment/pull/2) | `feature/control` → `main` | 종료 (base `main` 불가, 이후 제어 코드 대체) |
| [#17](https://github.com/visualkhh/Lv2_noon_Assignment/pull/17) | `feature/integration` → `main` — bag 녹화 압축(`recordings.zip`, 47MB)·`INTEGRATION.md`·소감 | 2026-10-08 종료 (팀장: main 직접 병합 불허). 녹화 자료는 노션으로 대체 |

**리뷰**: 다른 PR 리뷰 기록 없음

**구현·검증**
- [bag 구현,검증 다운로드](https://app.notion.com/p/teamsparta/A-_-3eb2dc3ef51480bc8878c20e26c1b61e?source=copy_link#3f32dc3ef514805e8508fc375e70fdd0)

**회고***
- 나는 통합 모듈 관리로 프로젝트로 작동하는 기기의 행적을 남기는 역할을 수행했다.
- 작동시킨 기기는 라즈베리파이 OpenCR 이 연동된 기기로 모터 & RealSense 였음.
- 재미있게도 개선될 점은 인공지능 사용량을 줄여야 한다는 것이다. 모두다 중독자다.
- 그래도 어제 시연할 때는 튜터님께 확실히 인정 받았고, 보기에도 만족스러웠다.
- 그런데 인공지능을 2년 다루면서 현재 시스템을 극복하는 방법은 AI 만드는 것만이다.
- 마지막 선택지는 블럭 코딩도 고려하고 있고, 아직 논의 중에 있다고 한다.
- 그리고 항목 중에서 필요한 것들만 선택해서 기재하면 될 것으로 생각하고 있다.

### 2.3 심규진 — 제어

**담당 범위**: 제어 노드(`dynamixel_move_node`·`dynamixel_controller`), OpenCR 펌웨어, Pi 제어 환경

**구현·검증**

| 내용 | 관련 문제 | 증빙 |
|---|---|---|
| OpenCR 펌웨어·Dynamixel 위치 제어, `config.h` 분리 | 3 | 커밋 [`f2bb3e8`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/f2bb3e8), [`6a0bb06`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/6a0bb06) |
| `/tracking_status` 발행 (IDLE·TRACKING·LOST) | 2·4 | PR [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) |
| udev 규칙(`/dev/opencr`)·Pi 설정 스크립트 `setup_pi.sh`, 상태 토픽 수정 | 4·5 | PR [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6) |
| 펌웨어 watchdog: 명령 500ms 미수신 시 위치 유지 (`COMMAND_TIMEOUT_MS`) | 4 (제어 통신 중단) | PR [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6), 커밋 [`3ee2bbc`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/3ee2bbc) |
| 모터 상태 피드백: 펌웨어 `S,…` 50ms 송신 → `/joint_states` | 3 (실측 위치) | PR [#14](https://github.com/visualkhh/Lv2_noon_Assignment/pull/14) |
| Kp A/B 시험 설정(`kp_pan_a.yaml`·`kp_pan_b.yaml`)·launch 인자, 문제 3 결과 정리, presentation 보강 | 3·전체 | PR [#16](https://github.com/visualkhh/Lv2_noon_Assignment/pull/16) (`docs/checklist`에 병합 — 통합 브랜치 반영 TODO) |

**병합된 PR**

| PR | 제목 | 리뷰 | 병합 |
|---|---|---|---|
| [#4](https://github.com/visualkhh/Lv2_noon_Assignment/pull/4) | add: add /tracking_status publisher | 김현하 라인 코멘트·질문 → 상태 전이 표로 답변 | 2026-10-05 (팀장) |
| [#6](https://github.com/visualkhh/Lv2_noon_Assignment/pull/6) | fix & add: udev rule setting and fix tracking_status | 김현하 코멘트·요청 → 스크립트·watchdog 반영 | 2026-10-05 (팀장) |
| [#14](https://github.com/visualkhh/Lv2_noon_Assignment/pull/14) | fix: recept motor state function add | 없음 | 2026-10-08 (팀장) |
| [#16](https://github.com/visualkhh/Lv2_noon_Assignment/pull/16) | `docs/test` → `docs/checklist` (Kp A/B 설정·문제 3·presentation) | 문태영 라인 코멘트 "해상도 축소 내용 반영된 거 확인" | 2026-10-08 (문태영) |

**작성한 리뷰**

| PR | 작성자 | 리뷰 내용 |
|---|---|---|
| [#15](https://github.com/visualkhh/Lv2_noon_Assignment/pull/15) | 문태영 | `README.md` 라인 코멘트(본인 작업 반영 확인), "체크리스트와 그래프, 문서작성 잘 되어있는거 같습니다. merge해도 될거 같습니다." |

**Issue**: 담당 [#5](https://github.com/visualkhh/Lv2_noon_Assignment/issues/5) [#8](https://github.com/visualkhh/Lv2_noon_Assignment/issues/8) [#9](https://github.com/visualkhh/Lv2_noon_Assignment/issues/9) · 작성 [#7](https://github.com/visualkhh/Lv2_noon_Assignment/issues/7) (파란색 객체 인식 수정 → 문태영). 모두 open — TODO(심규진): 해결된 Issue는 해당 PR을 연결해 닫기

**회고**

잘된 점
- 회의에서 논의했던 구조도와 워크플로우를 기반으로 개발 및 구현을 시작하니 순조롭게 진행되었다. 인지담당과 의견 합의가 잘 안되었을 때 구조도에 나온 플로우대로 토픽메시지와 노드를 맞춰서 하니 이견도 안나왔고 빠른 프로토타입 구축이 가능했던게 정말 잘 됐던 점이었음을 느꼈다.

어려웠던 점
- 실기계의 물리적인 문제들(연결문제, 토크문제 등)에 어려움을 겪었다. 소프트웨어 개발에서는 팀원들과 의견을 맞추며 함께 해결하는 과정으로 어려움을 극복했는데, 실기계 테스트 시에 장비의 예측할 수 없는 잔 오류들로 테스트 시 어려움을 많이 겪었었다.

해결
- 여러가지 통신 연결시도를 해보며 실기계의 카탈로그를 살펴보면서 AI에게도 질문하면서 하나하나 물리적인 오류들을 해결해 나갔다.

한계
- 여전히 실기계의 최적화 문제가 남아있다. tilt를 최고 한계점까지 실시해서 카메라 화각이 리버스로 들어올때 pan의 방향이 서로 뒤바뀌는 문제가 존재하고, panning 시 모터의 연결 선 길이 한계로 계속 panning할 수 없는 한계가 있다.

개선방안
- 로봇이 보는 화면이 거꾸로 되었을때 pan값을 바꿔주는 코드 개선을 해야할거 같다. panning을 계속 하려면 현재 로봇에 더해 기체 자체가 움직일 수 있도록 하는 개선방안이 필요하다.

### 2.4 문태영 — 인지

**담당 범위**: 인지 노드(`realsense` 패키지), 검출 규칙·시험 데이터, 카메라·환경 기록

**구현·검증**

| 내용 | 관련 문제 | 증빙 |
|---|---|---|
| 파란 사각 기둥 인지 노드 (HSV·Contour·대상 선택·중심 오차, `/target`) | 1·2 | 커밋 [`8d2693a`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/8d2693a), PR [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) |
| 토픽 이름 파라미터화 | 2·5 (재처리) | 커밋 [`70903bb`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/70903bb) |
| 검출 회귀 시험 gtest (실측 장면 3종) | 1 | [`test/test_detector.cpp`](ros2_ws/src/realsense/test/test_detector.cpp), [`test/data/`](ros2_ws/src/realsense/test/data/) |
| 카메라 기본 해상도 424×240 축소 (전송·처리 부담 감소) | 1·4 | Issue [#11](https://github.com/visualkhh/Lv2_noon_Assignment/issues/11) → PR [#12](https://github.com/visualkhh/Lv2_noon_Assignment/pull/12) |
| 카메라·환경·조명 조건 기록 | 1 | [results/perception_env_record.md](results/perception_env_record.md) |
| 문제별 결과 정리(`assignments/문제1~5`)·문제 3 데이터·그래프(`metrics.csv`·`plot_metrics.py`)·presentation 트러블슈팅 | 1~5 | PR [#15](https://github.com/visualkhh/Lv2_noon_Assignment/pull/15) |

**병합된 PR**

| PR | 제목 | 리뷰 | 병합 |
|---|---|---|---|
| [#1](https://github.com/visualkhh/Lv2_noon_Assignment/pull/1) | 인지 패키지(realsense) — 파란 사각 기둥 검출 및 topic 발행 | 김현하 코멘트 | 2026-10-04 (팀장) |
| [#12](https://github.com/visualkhh/Lv2_noon_Assignment/pull/12) | 카메라 기본 해상도 424x240으로 축소 (#11) | 없음 | 2026-10-06 (팀장) |
| [#15](https://github.com/visualkhh/Lv2_noon_Assignment/pull/15) | `docs/checklist` → `feature/control_perception` (문제별 결과·그래프·트러블슈팅) | 심규진 라인 코멘트·병합 의견 | 2026-10-08 (작성자 본인) |

**작성한 리뷰**

| PR | 작성자 | 리뷰 내용 |
|---|---|---|
| [#16](https://github.com/visualkhh/Lv2_noon_Assignment/pull/16) | 심규진 | `README.md` 라인 코멘트 "해상도 축소 내용 반영된 거 확인했습니다.", "수정된거 확인했습니다." → 병합 |

**Issue**: 담당 [#7](https://github.com/visualkhh/Lv2_noon_Assignment/issues/7) [#9](https://github.com/visualkhh/Lv2_noon_Assignment/issues/9) · 작성 [#11](https://github.com/visualkhh/Lv2_noon_Assignment/issues/11) (PR #12로 해결, open 상태 — TODO: 닫기)

**회고**
잘된점:
- 밝기에 따른 HSV 값 수정
- 감지된 객체 바운딩 박스 처리 및 객체 중심 좌표 구현
- 제어 파트에서 구독할 /target 토픽 구현

어려웠던 점:
- 통합과정에서 환경 구축을 위한 ROS2 명령어 사용법
- 통합 후 실행 파일 배포 과정에 대한 이해

한계:
- 강의 내용에 대한 이해를 바탕으로 프로젝트에 녹여내고 싶었지만, 역량 부족으로 AI 사용 빈도가 굉장히 높았음
- ROS2 /dev 설정, 가상환경 구축 및 실행 등 이해도 부족

개선 방향:
- 복습과 실습을 반복해 기초 다지기
- AI 사용하기 전 직접 최대한 목표에 맞게 구현해보기
- AI를 사용하더라도 구체적으로 어떤 내용을 구현했는지 파악해보기

## 3. 권한/보호 설정

- 저장소: 개인 계정 [`visualkhh/Lv2_noon_Assignment`](https://github.com/visualkhh/Lv2_noon_Assignment) (public)
- main 보호: Ruleset **"Protect main branch"** (id 24349052, active) — 2026-10-08 GitHub API(`/repos/…/rules/branches/main`)로 확인

| 항목 | 발제 기준 | 현재 설정 | 상태 |
|---|---|---|---|
| 저장소 위치 | SpartaPA 조직 `Lv2_팀명_과제` | 개인 계정 저장소 | TODO: 조직 저장소 이전·생성 또는 운영 안내에 따른 사유 기록 |
| 권한 | 팀장 Admin · 팀원 Write | 팀장 Admin (소유자) · 팀원 권한은 API로 확인 불가 | TODO: Settings → Collaborators 화면 확인 |
| Require a pull request before merging | 적용 | 적용 (`pull_request`) | ✅ |
| Require approvals (작성자 아닌 1명 이상) | 적용 | 1명 (`required_approving_review_count: 1`) | ✅ |
| Dismiss stale approvals | 적용 | 꺼짐 (`dismiss_stale_reviews_on_push: false`) | 운영 규칙: 새 커밋 후 재승인 ([CONTRIBUTING.md](../CONTRIBUTING.md#병합)) |
| Require conversation resolution | 적용 | 꺼짐 (`required_review_thread_resolution: false`) | 운영 규칙: 리뷰 대화 해결 후 병합 ([CONTRIBUTING.md](../CONTRIBUTING.md#병합)) |
| Restrict who can push to main (팀장) | 적용 | Restrict updates — bypass 권한자만 main 갱신 (`update`) | ✅ (bypass 대상은 TODO: 팀장만인지 화면 확인) |
| Do not allow bypassing | 적용 | bypass 권한자는 PR·승인 규칙도 건너뛸 수 있음 | ⚠️ 운영 규칙: 팀장도 bypass로 병합하지 않고 PR·승인을 거친다 ([CONTRIBUTING.md PR 규칙](../CONTRIBUTING.md#pr-규칙)) |
| force push 금지 | 적용 | 적용 (`non_fast_forward`) | ✅ |
| main 삭제 금지 | 적용 | 적용 (`deletion`) | ✅ |
| develop 보호 | (팀 흐름상 필요) | 규칙 없음 | 운영 규칙: develop PR도 같은 절차(타인 승인 → 팀장 병합) ([CONTRIBUTING.md](../CONTRIBUTING.md#브랜치)) |
| 보호 확인용 작은 문서 PR (승인 전 병합 불가 → 승인 → 팀장 병합) | 링크 기록 | — | TODO: PR 링크 기록 |

> 위 표의 "현재 설정"은 공개 API로 읽을 수 있는 값이다. 권한·bypass 대상은 공개 API로 보이지 않아 저장소 설정 화면으로 확인한다.

## 4. 대행·예외

**팀장 대행**: 없음 (기간·대행자·위임 권한 해당 없음). 계정·비밀번호·토큰을 공유해 대신 작업하지 않는다.

**절차 예외 (PR·승인 없이 반영된 변경)**

| 날짜 | 내용 | 사유 | 후속 조치 |
|---|---|---|---|
| 2026-10-04 | `chore/testbed-ci-setup` 브랜치를 PR 없이 병합 ([`d3dd081`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/d3dd081)) | 팀장 작업(테스트베드) 직접 병합 | 이후 팀장 작업은 PR + 타인 승인 |
| 2026-10-04 / 10-05 | `feature/control`(정구영)을 PR 없이 병합 ([`fd111c8`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/fd111c8), [`dc12c52`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/dc12c52)) | PR [#2](https://github.com/visualkhh/Lv2_noon_Assignment/pull/2)가 base `main`으로 올라와 종료 → 통합 브랜치로 직접 반영. 이후 제어 코드가 대체되어 **최종 코드에는 남지 않음** | — |
| 2026-10-05 | `feature/control_sim`을 PR 없이 병합 ([`9edc986`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/9edc986)) | 통합 시험 일정 | — |
| 2026-10-06 | 정구영이 작성한 bag 기록·재생 스크립트를 팀장 커밋으로 반영 ([`5990e17`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/5990e17)) | 전달받은 파일을 통합 브랜치에 바로 반영 | 작성자 표기는 [2.2](#22-정구영--통합-bag-기록재생)에 기록 |
| 2026-10-06 | PR [#13](https://github.com/visualkhh/Lv2_noon_Assignment/pull/13) — GitHub Copilot(봇) 작성 PR 병합 (CI 러너 Ubuntu 26.04 고정) | 팀장이 Copilot에 요청한 CI 설정 변경 | 팀원 개인 기여로 세지 않음 |
| 2026-10-04 ~ 10-08 | 병합된 PR 8건(#1 #4 #6 #12 #13 #14 #15 #16) 모두 Approve 없이 병합 (리뷰는 COMMENTED만) | 보호 규칙 미적용 상태 | [3](#3-권한보호-설정) 보호 규칙 적용, 이후 Approve 후 병합 |
| 2026-10-08 | PR [#15](https://github.com/visualkhh/Lv2_noon_Assignment/pull/15) 작성자(문태영)가 직접 병합, PR [#16](https://github.com/visualkhh/Lv2_noon_Assignment/pull/16)을 팀장이 아닌 문태영이 병합 | 문서 브랜치 간 병합, 리뷰 코멘트 후 진행 | [CONTRIBUTING.md PR 규칙](../CONTRIBUTING.md#pr-규칙): 병합은 팀장 |
| 2026-10-08 | PR [#17](https://github.com/visualkhh/Lv2_noon_Assignment/pull/17)(정구영) base `main` → 종료 | main은 `develop`에서만 병합 | 녹화 자료는 노션, 소감은 [2.2](#22-정구영--통합-bag-기록재생)에 반영 |
| 2026-10-08 | 팀장이 제어 담당 코드(`dynamixel_controller`)에 시리얼 원본 토픽 추가 ([`b48dee7`](https://github.com/visualkhh/Lv2_noon_Assignment/commit/b48dee7)) | 실기 펌웨어 입출력 확인용 | 담당자(심규진) 리뷰 |

## 5. 통합 확인

### 5.1 팀장 최종 통합 확인

| 항목 | 확인 | 근거 |
|---|---|---|
| 테스트베드 `test-all` (serial·firmware·bringup) | ✅ 2026-10-08 ALL PASS (로컬 테스트베드) | [devops.md 2.3](devops.md#23-test-all-상세) |
| CI x86_64·aarch64 | TODO: 최신 커밋 실행 결과 링크 | [Actions](https://github.com/visualkhh/Lv2_noon_Assignment/actions) |
| 실기 통합 동작 (카메라 → 검출 → 제어 → 모터) |  ✅ | [report.md](report.md) |
| 안전 정지 (미검출·인지 입력 중단·제어 통신 중단) |  ✅ | |
| 4인 모두 병합 PR 1건 이상 | 심규진·문태영 ✅ · 김현하 통합 PR 예정 · 정구영 미충족 | [1.1](#11-요구사항-충족-현황-2026-10-08) |
| 4인 모두 타인 PR 리뷰 1건 이상 | 김현하·심규진·문태영 ✅ · 정구영 미충족 | [1.1](#11-요구사항-충족-현황-2026-10-08) |
| 팀장 PR 타인 승인 | ⏳ 통합 PR(→ develop, → main)에서 | [2.1](#21-김현하--팀장--테크리드--통합) |
| README 재현 확인 |  ✅ | 5.1 |
| 최종 main 병합 (`develop` → `main` PR) · 제출 태그 `lv2-module5-submit` | ✅  | |

- 확인자: 김현하
