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

[현재상태]
- 로컬의 `scene_20261006_101831`, `scene2`는 `/rosout`만 녹화된 빈 bag (각 약 7초·11초, 4.0K) → 목록에서 제외
- 로봇 bringup 후 다시 녹화 필요
- 빈 bag은 폴더·`.tar.gz`·`SHA256SUMS` 해당 줄 삭제

[목록]
- 등록된 bag 없음 (녹화하면 아래에 [목록:파일명] 항목이 자동 추가됨)
