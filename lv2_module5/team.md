# 팀 구성과 기여 기록

팀원별 역할·기여·Issue·병합 PR·리뷰를 기록합니다.
각 팀원은 **병합된 PR 1개 이상**과 **다른 팀원 PR에 남긴 구체적인 리뷰 1개 이상**을 아래에 링크로 연결합니다.

## 팀 요약

| 이름 | GitHub ID | 역할 | 담당 모듈 | 병합 PR 수 | 리뷰 수 |
|---|---|---|---|---|---|
| 김현하 | visualkhh | 팀장 | 테크 리드, 문서 통합, 테스트베드 | TODO | TODO |
| 정구영 | TODO | 팀원 | 통합 | TODO | TODO |
| 심규진 | TODO | 팀원 | 제어 | TODO | TODO |
| 문태영 | TODO | 팀원 | 인지 | TODO | TODO |

> git 커밋 작성자로 보이는 ID: `visualkhh`, `a71143055`, `Teewhy-M`, `Gyujion`, `Ubuntu`(Pi 기본 사용자 — git `user.name` 설정 필요). TODO: 각자 본인 ID 기입.
> 병합 PR 이력 (git merge 커밋 기준, 2026-10-05): #1 `feature/perception` (2026-10-04), #4 `feature/control_sim` (2026-10-05). PR 작성자·리뷰어는 GitHub에서 확인 후 기입.

---

## 김현하 — 팀장 · 테크 리드 · 문서 통합

### 담당 범위
- 테크 리드: 저장소·브랜치·PR 규칙(CONTRIBUTING.md), 공통 인터페이스 합의, 최종 병합·태그
- 테스트베드: Docker(Ubuntu 26.04 + Lyrical), 가상 시리얼, `test-*` 검사, CI
- 통합 지원: `bringup`·`fake_camera_bringup` 패키지, 통제실·3D 시뮬레이션, `firmware/upload.sh`
- 문서 통합: README·report·test-checklist

### 주요 기여
| 내용 | 관련 문제 | 증빙 (코드·로그·결과 파일) |
|---|---|---|
| Docker 테스트베드 + 가상 시리얼 + 검사 스크립트 | 전체·5 (재현) | `docker/`, `test-all` |
| 실기·테스트베드 bringup 묶음 | 2 | `ros2_ws/src/bringup`, `ros2_ws/src/fake_camera_bringup` |
| 통제실·3D 닫힌 루프 시뮬레이션 (대체 환경) | 2·3·4 리허설 | `docker/test-controller/` |
| Pi용 펌웨어 컴파일·업로드 스크립트 | 3 | `firmware/upload.sh` |
| 시험 체크리스트·보고서 틀 | 전체 | `test-checklist.md`, `report.md` |

### Issue
| 번호 | 제목 | 상태 |
|---|---|---|
| #  | | |

### 병합된 PR
| 번호 | 제목 | 리뷰어 | 병합일 |
|---|---|---|---|
| #  | | | |

### 작성한 리뷰
| PR | 작성자 | 리뷰 요약 (구체적 지적·제안) | 링크 |
|---|---|---|---|
| #  | | | |

### 회고
- 잘된 점:
- 어려웠던 점과 해결:
- 한계·개선 방향:

---

## 정구영 — 통합 담당

### 담당 범위
- TODO(정구영): ROS2 인터페이스·실행 구성(launch), bag 기록·재생, 재현 문서

### 주요 기여
| 내용 | 관련 문제 | 증빙 (코드·로그·결과 파일) |
|---|---|---|
| | | |

### Issue
| 번호 | 제목 | 상태 |
|---|---|---|
| #  | | |

### 병합된 PR
| 번호 | 제목 | 리뷰어 | 병합일 |
|---|---|---|---|
| #  | | | |

### 작성한 리뷰
| PR | 작성자 | 리뷰 요약 (구체적 지적·제안) | 링크 |
|---|---|---|---|
| #  | | | |

### 회고
- 잘된 점:
- 어려웠던 점과 해결:
- 한계·개선 방향:

---

## 심규진 — 제어 담당

### 담당 범위
- 제어: `ros2_ws/src/dynamixel` (dynamixel_move_node, dynamixel_controller), OpenCR 펌웨어 `firmware/opencr_pan_tilt`
- TODO(심규진): 상세 기입

### 주요 기여
| 내용 | 관련 문제 | 증빙 (코드·로그·결과 파일) |
|---|---|---|
| | | |

### Issue
| 번호 | 제목 | 상태 |
|---|---|---|
| #  | | |

### 병합된 PR
| 번호 | 제목 | 리뷰어 | 병합일 |
|---|---|---|---|
| #4 | feature/control_sim → (OpenCR 펌웨어·Dynamixel 위치 제어) | TODO | 2026-10-05 | <!-- TODO: 작성자 본인 PR인지 확인 -->

### 작성한 리뷰
| PR | 작성자 | 리뷰 요약 (구체적 지적·제안) | 링크 |
|---|---|---|---|
| #  | | | |

### 회고
- 잘된 점:
- 어려웠던 점과 해결:
- 한계·개선 방향:

---

## 문태영 — 인지 담당

### 담당 범위
- 인지: `ros2_ws/src/realsense` (perception_node, HSV·Contour, 디버그 영상, gtest), 환경·카메라 기록 `results/perception_env_record.md`
- TODO(문태영): 상세 기입

### 주요 기여
| 내용 | 관련 문제 | 증빙 (코드·로그·결과 파일) |
|---|---|---|
| | | |

### Issue
| 번호 | 제목 | 상태 |
|---|---|---|
| #  | | |

### 병합된 PR
| 번호 | 제목 | 리뷰어 | 병합일 |
|---|---|---|---|
| #1 | feature/perception → (파란 사각 기둥 인지 노드) | TODO | 2026-10-04 | <!-- TODO: 작성자 본인 PR인지 확인 -->

### 작성한 리뷰
| PR | 작성자 | 리뷰 요약 (구체적 지적·제안) | 링크 |
|---|---|---|---|
| #  | | | |

### 회고
- 잘된 점:
- 어려웠던 점과 해결:
- 한계·개선 방향:

---

## 저장소 권한·main 보호 설정

PDF 기준: SpartaPA 조직의 `Lv2_팀명_과제` 저장소 1개. 팀장 Admin, 팀원 Write, main은 PR + 타인 승인 1개 이상 + 팀장만 병합.

| 항목 | 설정 | 확인 (화면·PR 링크) |
|---|---|---|
| 저장소 위치 | ⚠️ 현재 개인 계정 `visualkhh/Lv2_noon_Assignment` → TODO(팀장): SpartaPA 조직 저장소로 이전 또는 생성 | |
| 권한 | 팀장 Admin · 팀원 Write | TODO |
| Require a pull request before merging | TODO | |
| Require approvals (작성자 아닌 1명 이상) | TODO | |
| Dismiss stale approvals | TODO | |
| Require conversation resolution | TODO | |
| Restrict who can push to main (팀장) | TODO | |
| Do not allow bypassing | TODO | |
| force push·main 삭제 금지 | TODO | |
| 적용 불가 항목과 운영 규칙 | TODO (정책상 안 되는 항목이 있으면 사유와 대체 규칙) | |

> ⚠️ 브랜치 흐름: PDF는 "PR의 base는 **main**", `CONTRIBUTING.md`는 develop 경유. TODO(팀장): 하나로 정하고 CONTRIBUTING.md 갱신.
> 보호 규칙 확인용 작은 문서 PR 링크: TODO

## 대행·예외

| 기간 | 대행자 | 위임 권한 | 사유 |
|---|---|---|---|
| (없으면 "없음") | | | |

> 계정·비밀번호·토큰을 공유하여 대신 작업하지 않습니다.

## README 재현 확인 (문제 5)

작성자가 아닌 팀원이 `lv2_module5/README.md`만 보고 실행한 기록입니다.

| 확인자 | 날짜 | 기준 커밋 | 재현 항목 | 결과 | 수정한 누락 사항 |
|---|---|---|---|---|---|
| | | | | | |

## 팀장 최종 통합 확인

| 항목 | 확인 | 비고 |
|---|---|---|
| 전체 시스템 통합 동작 (카메라 → 검출 → 제어 → 모터) | ☐ | |
| 안전 정지 (미검출·토픽 중단·제어 통신 중단) | ☐ | |
| 4인 모두 병합 PR 1개 이상 | ☐ | |
| 4인 모두 타인 PR 리뷰 1개 이상 | ☐ | |
| README 재현 확인 완료 | ☐ | |
| 제출 태그 `lv2-module5-submit` 생성 | ☐ | |

- 확인자: 김현하
- 확인 날짜:
- 기준 커밋:
