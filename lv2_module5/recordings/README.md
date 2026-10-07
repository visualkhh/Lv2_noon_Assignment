[Recordings:RosBagList]

[개요]
- 이 폴더만 있으면 어떤 기기에서든 동작 (저장소 전체, zip으로 받은 recordings 단독 모두)
- ROS 배포판은 기기마다 자동 탐지 (lyrical·kilted·jazzy·humble·소스 빌드) — 스크립트에 배포판 고정 없음
- bag은 `bags/`에 모음: 원본 폴더는 Git·zip에 넣지 않고, `.tar.gz` + `SHA256SUMS` + 아래 [목록]으로 관리
- 무결성 확인: `cd recordings && sha256sum -c SHA256SUMS`

[처음 한 번 — 연동]
- `./connect.sh <상대 주소>` — 예: `./connect.sh agumon.local --domain 9`
- 기기 정보 → ROS 탐지·패키지 점검 → 워크스페이스 빌드 → VS Code 경로 → 상대 토픽 확인 → `.link.env` 저장
- 상대 없이 이 PC만: `./connect.sh --local`
- 상대도 이 PC 주소로 연결해야 함 (Wi-Fi가 멀티캐스트를 막아 `ROS_STATIC_PEERS`로 서로 지정)
- ROS가 Docker 컨테이너에만 있으면: `./connect.sh --docker <컨테이너> <상대 주소>`
- zip으로 받아 실행 권한이 없으면: `bash connect.sh` (줄바꿈·권한을 스스로 고침)

[호환 기준]
- RMW `rmw_fastrtps_cpp`로 통일 — jazzy ↔ lyrical 양방향 통신 확인 (2026-10-07, 같은 PC)
- bag 저장 형식 `mcap`(metadata v9) — jazzy ↔ lyrical 서로 읽기 확인
- 녹화 토픽은 `--topics`로 지정 (lyrical은 토픽 위치 인자를 받지 않음, jazzy도 `--topics` 지원)
- `ros2 topic list`는 `--no-daemon`으로 호출 — 한 PC에 배포판이 둘이면 daemon끼리 충돌
- 기본은 로컬 모드(`ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST`), 연동은 `LINK=1`·`record_peer.sh`에서만
  → 가상 실행·재생의 `/motor_cmd`가 실수로 로봇에 가지 않음

[스크립트]
- `connect.sh` — 연동·점검 (위 참고)
- `run_and_record.sh [이름] [launch 인자]` — 빌드 → 노드 실행(가상↔실기 자동 전환) → 녹화(Ctrl+C로 종료) → 압축·체크섬 → [목록] 등록
  - 이름 기본 `sim_YYYYmmdd_HHMMSS`, `NO_RECORD=1` 실행만, `RECORD_RAW=0` 디버그 영상만, `LINK=1` 상대와 같은 네트워크
  - 워크스페이스(`../ros2_ws` 또는 `--ws`로 저장한 경로) 필요
- `record_peer.sh [이름] [토픽...]` — 연동한 상대 기기의 토픽을 녹화만 (노드 실행 없음) → 압축·등록
- `record_agumon.sh [토픽...]` — 라즈베리파이(`agumon.local`, 도메인 9) 바로가기
- `record_scene.sh [이름] [토픽...]` — 이 PC에서 따로 띄운 노드를 녹화 → 압축·등록
- `play_bag.sh <이름> [옵션]` — 재생 (폴더가 없으면 `.tar.gz`를 체크섬 확인 후 풀어서), `LINK=1`이면 상대 쪽으로 (`/motor_cmd` 제외)
- `pack_bag.sh <이름>` — `bags/<이름>` 압축 + `SHA256SUMS` 기록 (같은 파일 줄은 교체)
- `register_bag.sh <이름> [장면]` — `metadata.yaml`로 [목록] 등록 (기존 장면 설명·다운로드 링크 유지)
- `share_zip.sh` — 팀원에게 보낼 zip (스크립트·문서·`.tar.gz`, 원본 폴더·`.link.env` 제외), `NO_BAGS=1`이면 스크립트·문서만
- `env.sh` — 공통 환경 (직접 실행하지 않음, 터미널에서 같은 설정을 쓰려면 `source env.sh --link`)
- `../ros2_ws/sim_gui.sh` — 웹 GUI 서버를 띄우고 `run_and_record.sh` 실행

[기록]
- 녹화 일시·장면·기간·토픽·용량·SHA256·기준 커밋(녹화 시각 직전 커밋)은 자동 기입
- `.tar.gz` 업로드 후 `- 다운로드: (업로드 후 기입)`만 직접 링크로 수정 (재등록해도 링크는 유지)
- `SHA256SUMS`·`README.md` 커밋 (git 작업은 직접)

[정리 기록]
- 2026-10-07: bag을 `bags/`로 이동, `SHA256SUMS` 경로를 `bags/...`로 변경
- `/rosout`만 녹화된 빈 bag 7개(`scene2`·`scene3`·`scene4`·`scene_20261006_101831`·`scene_20261006_105352`·`scene_20261006_165159`·`scene_20261006_175416`)
  → `bags/_empty/`로 이동, 목록·`SHA256SUMS`에서 제외 (체크섬은 `bags/_empty/SHA256SUMS`), 필요 없으면 폴더째 삭제
- 압축·등록이 빠져 있던 `sim_20261006_182618`·`sim_20261006_183117`, 등록이 빠져 있던 `agumon_20261006_194432` 추가

[목록]

[목록:sim_20261006_174201.tar.gz]
- 녹화 일시: 2026-10-06 17:42:03
- 장면: sim_gui 자동 녹화 (카메라: virtual, OpenCR: disconnected)
- 기간: 55.5s
- 토픽: /camera_source(55), /motor_cmd(706), /opencr_status(55), /perception_node/debug_image/compressed(415), /target(831), /tracking_status(60)
- 용량: 2.1M
- SHA256: `3ca4b6f615b0dd5fb144ba680e60aa9cfd3875d81f9f200978141b6dbde4338c`
- 기준 커밋: `d1ea7e3`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_174624.tar.gz]
- 녹화 일시: 2026-10-06 17:46:25
- 장면: sim_gui 자동 녹화 (카메라: virtual, OpenCR: disconnected)
- 기간: 52.3s
- 토픽: /camera_source(52), /joint_states(786), /motor_cmd(666), /opencr_status(52), /perception_node/debug_image/compressed(393), /target(786), /tracking_status(57), /virtual_target(696)
- 용량: 2.0M
- SHA256: `30c8eac608939277d9c040484a9ac518e20ba37dd51ebf2a26a7bc1fff9b6e12`
- 기준 커밋: `0fb218f`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_174730.tar.gz]
- 녹화 일시: 2026-10-06 17:47:31
- 장면: sim_gui 자동 녹화 (카메라: virtual, OpenCR: disconnected)
- 기간: 73.8s
- 토픽: /camera_source(73), /joint_states(1108), /motor_cmd(917), /opencr_status(74), /perception_node/debug_image/compressed(554), /target(1108), /tracking_status(81), /virtual_target(971)
- 용량: 2.8M
- SHA256: `ef1eaccfd106d50e82f511240a0b7d5c75cac1426a5c88536b8b4e7dbd2db682`
- 기준 커밋: `0fb218f`
- 다운로드: (업로드 후 기입)

[목록:scene1.tar.gz]
- 녹화 일시: 2026-10-06 17:55:07
- 장면: -
- 기간: 11.3s
- 토픽: /camera/camera/color/image_raw(170), /motor_cmd(170), /target(170), /tracking_status(11)
- 용량: 1.2M
- SHA256: `845ff0258907574299d21ce03500d7a47b39fd1a23e2fd68f7a1ffa4c386c5bc`
- 기준 커밋: `e607399`
- 다운로드: (업로드 후 기입)

[목록:scene5.tar.gz]
- 녹화 일시: 2026-10-06 18:02:44
- 장면: -
- 기간: 5.7s
- 토픽: /camera/camera/color/image_raw(85), /motor_cmd(87), /target(87), /tracking_status(6)
- 용량: 580K
- SHA256: `5559f860a2f3e5e89c2e6ccf3b3a19fd6ee0f147be9f6665845c31ccb43f60c4`
- 기준 커밋: `cd0662d`
- 다운로드: (업로드 후 기입)

[목록:scene6.tar.gz]
- 녹화 일시: 2026-10-06 18:05:27
- 장면: -
- 기간: 2.1s
- 토픽: /camera/camera/color/image_raw(30), /motor_cmd(32), /target(32), /tracking_status(2)
- 용량: 208K
- SHA256: `0804782af2d8db8c74a1bb43e9f70d376b6d1620047bb5daaec8223acd8ded4f`
- 기준 커밋: `210fbd8`
- 다운로드: (업로드 후 기입)

[목록:scene7.tar.gz]
- 녹화 일시: 2026-10-06 18:11:43
- 장면: -
- 기간: 15.5s
- 토픽: /camera/camera/color/image_raw(226), /motor_cmd(219), /target(218), /tracking_status(17)
- 용량: 1.5M
- SHA256: `5c62ea716ec7d1c6b583d9b319b041ce4d0ee5c67eec627c626d53e5e7dbed43`
- 기준 커밋: `9e63539`
- 다운로드: (업로드 후 기입)

[목록:scene8.tar.gz]
- 녹화 일시: 2026-10-06 18:12:15
- 장면: -
- 기간: 27.1s
- 토픽: /camera/camera/color/image_raw(406), /motor_cmd(336), /target(407), /tracking_status(29)
- 용량: 2.7M
- SHA256: `403a9572e48be788f438ba03042d404fda4bcf0e08af014286a9c0bcc8257e75`
- 기준 커밋: `9e63539`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_182246.tar.gz]
- 녹화 일시: 2026-10-06 18:22:50
- 장면: -
- 기간: 7.3s
- 토픽: /camera/camera/color/image_raw(111), /motor_cmd(111), /target(91), /tracking_status(7)
- 용량: 756K
- SHA256: `5406841431f07cfb84c2b65ae35297702f3f13d80cdbf0756b487ed46aa99784`
- 기준 커밋: `27f1c78`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_182618.tar.gz]
- 녹화 일시: 2026-10-06 18:26:22
- 장면: (카메라: virtual, OpenCR: disconnected)
- 기간: 4.2s
- 토픽: /camera/camera/color/image_raw(62), /camera_source(4), /joint_states(62), /motor_cmd(64), /opencr_status(4), /target(49), /tracking_status(4), /virtual_target(62)
- 용량: 440K
- SHA256: `fc8d7c414c1b8b8df4b3f61f2bf40b1c91218ddf1c86a413b7098b4695398991`
- 기준 커밋: `2cf91cd`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_183117.tar.gz]
- 녹화 일시: 2026-10-06 18:31:21
- 장면: (카메라: virtual, OpenCR: disconnected)
- 기간: 17.3s
- 토픽: /camera/camera/color/image_raw(230), /camera_source(15), /joint_states(239), /motor_cmd(224), /opencr_status(17), /target(260), /tracking_status(18), /virtual_target(194)
- 용량: 1.6M
- SHA256: `b7e08f202ded9011e69b532ee44052b1a09219a3c8086bfb0d24f6879fe60351`
- 기준 커밋: `c5aa13b`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_183814.tar.gz]
- 녹화 일시: 2026-10-06 18:38:19
- 장면: (카메라: virtual, OpenCR: disconnected)
- 기간: 77.7s
- 토픽: /camera/camera/color/image_raw(1167), /camera_source(78), /joint_states(1167), /motor_cmd(935), /opencr_status(78), /target(1167), /tracking_status(86), /virtual_target(987)
- 용량: 7.8M
- SHA256: `7217bf0a4b18ef0684865d2719dec8c92bd4b4982818e6db4fd02fbda887189a`
- 기준 커밋: `d4a4e09`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_184930.tar.gz]
- 녹화 일시: 2026-10-06 18:49:34
- 장면: (카메라: virtual, OpenCR: disconnected)
- 기간: 4.3s
- 토픽: /camera/camera/color/image_raw(65), /camera_source(5), /joint_states(65), /motor_cmd(65), /opencr_status(5), /target(65), /tracking_status(5), /virtual_target(65)
- 용량: 452K
- SHA256: `b6cdea236140f53057d1c55c458b0f89942f29a6c68a7a0b1e26aa7fe60d66ab`
- 기준 커밋: `e175123`
- 다운로드: (업로드 후 기입)

[목록:agumon_20261006_194432.tar.gz]
- 녹화 일시: 2026-10-06 19:44:34
- 장면: 라즈베리파이(agumon.local) 원격 녹화
- 기간: 18.8s
- 토픽: /target(563), /tracking_status(20)
- 용량: 20K
- SHA256: `41cd8c7151638fa6971a7c134ad5d35e2975a0e19d044a3107ca30014d9d32c6`
- 기준 커밋: `54cecdf`
- 다운로드: (업로드 후 기입)

[목록:peertest_tmp.tar.gz]
- 녹화 일시: 2026-10-07 10:18:21
- 장면: 연동 테스트
- 기간: 7.4s
- 토픽: /motor_cmd(75), /target(75), /tracking_status(15)
- 용량: 8.0K
- SHA256: `8c96cfe44e154775741747cab6b07fbb0d382e956f7298578e7f3bbb709e9133`
- 기준 커밋: `d0b93e5`
- 다운로드: (업로드 후 기입)
