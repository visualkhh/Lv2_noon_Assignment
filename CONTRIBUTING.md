# Contributing

Lv2_noon_Assignment 팀의 Git 협업 규칙입니다. 모든 팀원은 아래 규칙에 따라 브랜치를 만들고 커밋·PR을 작성합니다.

## 브랜치

| 브랜치 | 용도 | 예시 |
|---|---|---|
| `main` | 제출 가능한 안정 브랜치 (제출 태그 기준) | — |
| `develop` | 다음 통합을 위한 기본 개발 브랜치 | — |
| `feature/` (`feat/`) | 새로운 기능 개발 | `feature/hsv-detector` |
| `bugfix/` (`fix/`) | 버그 수정 | `fix/target-sign-inverted` |
| `hotfix/` | `main`의 긴급 수정 | `hotfix/opencr-timeout` |
| `release/` | 제출 전 검증·정리 | `release/lv2-module5-submit` |
| `chore/` | 빌드·설정·패키지 등 비코드 작업 | `chore/colcon-setup` |

작업 흐름:

1. Issue를 생성하고 담당자를 지정합니다.
2. `develop`에서 작업 브랜치를 생성합니다.
3. 작업 후 `develop`으로 PR을 올리고, 다른 팀원 1명 이상의 리뷰 승인 후 병합합니다.
4. 제출 시점에 `develop` → `main` 병합 후 `lv2-module5-submit` 태그를 생성합니다.

## 커밋 메시지

**타입**

| 타입 | 의미 |
|---|---|
| `feat` | 새로운 기능 추가 |
| `fix` | 버그 수정 |
| `docs` | 문서 수정 (README.md, 주석 등) |
| `style` | 코드 포맷팅 등 (로직 변경 없음) |
| `refactor` | 기능 변화 없는 코드 구조 변경 |
| `test` | 테스트 코드 추가 및 리팩토링 |
| `chore` | 빌드, 패키지 매니저 설정, 인프라 변경 등 |

**구조**

보통은 제목만 작성하고, 상세한 설명이 필요하면 본문과 바닥글을 포함한 3단 구조를 사용합니다.

```
<type>: <제목>

<본문 — 무엇을, 왜 변경했는지>

<바닥글 — 관련 Issue 등>
```

예시:

```
feat: HSV 마스크 기반 목표 검출 노드 추가

- HSV 범위·최소 면적을 config/detector.yaml로 분리
- 미검출 시 z=0으로 /target 발행

Closes #3
```
