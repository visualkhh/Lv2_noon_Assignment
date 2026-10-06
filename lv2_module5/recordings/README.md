[Recordings:RosBagList]

[개요]
- bag 원본은 용량 때문에 저장소에 포함하지 않고, 아래 목록의 링크에서 내려받음
- 무결성 확인: `cd recordings && sha256sum -c SHA256SUMS`

[준비]
- 다른 터미널에서 로봇 bringup·카메라 노드를 먼저 실행
- `ros2 topic list`로 토픽이 보이는지 확인
- `/parameter_events`, `/rosout`만 보이면 데이터가 없는 상태 → bringup, 네트워크, `ROS_DOMAIN_ID` 확인

[설정]
- `record_scene.sh` 맨 위 설정 칸만 수정
- `SCENE_NAME="scene3"` — bag 이름, `""`로 비우면 scene1, scene2 … 중 빈 번호 자동
- `DEFAULT_TOPICS=(/image_raw /odom)` — `ros2 topic list` 결과에 맞춰 수정 (예: `/camera/image_raw`)
- `SCENE_DESC=""` — 목록의 '장면' 칸, 비워 두면 녹화 후 입력받음
- 녹화가 끝난 이름은 다시 쓸 수 없으므로 다음 녹화 전에 `SCENE_NAME` 변경

[녹화]
- `cd lv2_module5/recordings`
- `./record_scene.sh` — 설정값으로 녹화, Ctrl+C로 종료
- `./record_scene.sh scene5 /image_raw /odom` — 인자를 주면 설정값 대신 사용
- 종료 후 자동으로 `tar.gz` 압축, `SHA256SUMS` 기록, 아래 [목록]에 항목 등록

[기록]
- 장면·기간·토픽·용량·SHA256·기준 커밋은 자동 기입, 같은 파일을 다시 등록하면 그 항목을 교체
- `.tar.gz` 업로드 후 `- 다운로드: (업로드 후 기입)`만 직접 링크로 수정
- `SHA256SUMS`·`README.md` 커밋 (git 작업은 직접)

[스크립트]
- `record_scene.sh` — ROS 환경·토픽 확인 → `pack_bag.sh` 호출 → README [목록]에 등록
- 데이터 토픽이 없는 bag은 README 등록 전에 한 번 더 확인
- `pack_bag.sh` — `ros2 bag record` → `tar.gz` 압축 → `SHA256SUMS` 기록 → 정보 출력
- 같은 bag을 다시 압축하면 `SHA256SUMS`의 기존 줄을 교체 (중복 없음)
- ROS 기본 토픽 외에 녹화된 데이터가 없으면 `⚠ 경고` 출력
- `.gitignore`가 허용 목록 방식이라 `record_scene.sh`를 올리려면 `!record_scene.sh` 추가 필요
- `../ros2_ws/sim_gui.sh` — 가상환경·실기(자동 전환) + 웹 GUI 실행 중 자동 녹화, Ctrl+C로 끝내면 `register_bag.sh`로 등록 (이름 `sim_YYYYmmdd_HHMMSS`, 장면 칸에 카메라·OpenCR 상태 자동 기입)
- `register_bag.sh <이름> [설명]` — 녹화된 bag을 `pack_bag.sh`로 압축·체크섬 후 README [목록]에 등록

[현재상태]
- 로컬의 `scene_20261006_101831`, `scene2`는 `/rosout`만 녹화된 빈 bag (각 약 7초·11초, 4.0K) → 목록에서 제외
- 로봇 bringup 후 다시 녹화 필요
- 빈 bag은 폴더·`.tar.gz`·`SHA256SUMS` 해당 줄 삭제

[목록]

[목록:scene3.tar.gz]
- 장면: -
- 기간: 0.0s
- 토픽: (데이터 없음)
- 용량: 4.0K
- SHA256: `c82afd41e28abf200abd16ec3d06cef8624f550ab73ca254c1f43e161d1b0701`
- 기준 커밋: `6aae1c0`
- 다운로드: (업로드 후 기입)

[목록:scene4.tar.gz]
- 장면: -
- 기간: 0.0s
- 토픽: (데이터 없음)
- 용량: 4.0K
- SHA256: `06d9fd745562116fa76c0a9478ca3c70ef48d5c529db04bb6cf22ab1b2488b2a`
- 기준 커밋: `6aae1c0`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_174201.tar.gz]
- 장면: sim_gui 자동 녹화 (카메라: virtual, OpenCR: disconnected)
- 기간: 55.5s
- 토픽: /camera_source(55), /motor_cmd(706), /opencr_status(55), /perception_node/debug_image/compressed(415), /target(831), /tracking_status(60)
- 용량: 2.1M
- SHA256: `3ca4b6f615b0dd5fb144ba680e60aa9cfd3875d81f9f200978141b6dbde4338c`
- 기준 커밋: `d1ea7e3`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_174624.tar.gz]
- 장면: sim_gui 자동 녹화 (카메라: virtual, OpenCR: disconnected)
- 기간: 52.3s
- 토픽: /camera_source(52), /joint_states(786), /motor_cmd(666), /opencr_status(52), /perception_node/debug_image/compressed(393), /target(786), /tracking_status(57), /virtual_target(696)
- 용량: 2.0M
- SHA256: `30c8eac608939277d9c040484a9ac518e20ba37dd51ebf2a26a7bc1fff9b6e12`
- 기준 커밋: `0fb218f`
- 다운로드: (업로드 후 기입)

[목록:sim_20261006_174730.tar.gz]
- 장면: sim_gui 자동 녹화 (카메라: virtual, OpenCR: disconnected)
- 기간: 73.8s
- 토픽: /camera_source(73), /joint_states(1108), /motor_cmd(917), /opencr_status(74), /perception_node/debug_image/compressed(554), /target(1108), /tracking_status(81), /virtual_target(971)
- 용량: 2.8M
- SHA256: `ef1eaccfd106d50e82f511240a0b7d5c75cac1426a5c88536b8b4e7dbd2db682`
- 기준 커밋: `0fb218f`
- 다운로드: (업로드 후 기입)

[목록:scene1.tar.gz]
- 장면: -
- 기간: 11.3s
- 토픽: /camera/camera/color/image_raw(170), /motor_cmd(170), /target(170), /tracking_status(11)
- 용량: 1.2M
- SHA256: `845ff0258907574299d21ce03500d7a47b39fd1a23e2fd68f7a1ffa4c386c5bc`
- 기준 커밋: `e607399`
- 다운로드: (업로드 후 기입)

[목록:scene5.tar.gz]
- 장면: -
- 기간: 5.7s
- 토픽: /camera/camera/color/image_raw(85), /motor_cmd(87), /target(87), /tracking_status(6)
- 용량: 580K
- SHA256: `5559f860a2f3e5e89c2e6ccf3b3a19fd6ee0f147be9f6665845c31ccb43f60c4`
- 기준 커밋: `cd0662d`
- 다운로드: (업로드 후 기입)

[목록:scene6.tar.gz]
- 장면: -
- 기간: 2.1s
- 토픽: /camera/camera/color/image_raw(30), /motor_cmd(32), /target(32), /tracking_status(2)
- 용량: 208K
- SHA256: `0804782af2d8db8c74a1bb43e9f70d376b6d1620047bb5daaec8223acd8ded4f`
- 기준 커밋: `210fbd8`
- 다운로드: (업로드 후 기입)
