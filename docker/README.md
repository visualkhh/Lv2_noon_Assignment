docker
---

# docker start
```shell
cd docker
docker compose build
docker compose up -d  
# 또는 한번에 docker compose up -d --build

docker ps
docker compose exec lyrical test-fake_camera_bringup 0
# 그냥 bash  docker compose exec lyrical bash
```

# docker down
```shell
docker compose down
docker ps
```


# preview debug controller
```shell
cd docker/test-controller
python3 -m venv .venv
source ./.venv/bin/activate
pip install -r requirements.txt
python run-controller.py
```


# 문제 해결

## `docker: unknown command: docker compose`

Compose v2 플러그인이 설치되지 않은 상태입니다. (`docker-compose`(하이픈)는 지원이 끝난 v1이라 쓰지 않습니다.)

```shell
docker compose version          # 확인 — 실패하면 아래 설치
sudo apt install docker-compose-v2
```

## `PermissionError: [Errno 13] Permission denied: '.../lv2_module5/debug/...'`

`run-controller.py` 실행 시 발생합니다.

- 원인: `lv2_module5/debug`는 컨테이너에 마운트되는 폴더입니다. 호스트에 없으면 Docker가 **root 소유**로 만들고, 컨테이너(root)가 그 안에 파일을 씁니다. 호스트 사용자로 실행한 `run-controller.py`는 여기에 쓸 수 없습니다.
- 확인:

```shell
ls -ld lv2_module5/debug lv2_module5/debug/*   # 소유자가 root면 이 문제
```

- 해결 (컨테이너가 떠 있을 때, sudo 불필요):

```shell
docker exec lv2_lyrical chown -R $(id -u):$(id -g) /ws/debug
```

- 해결 (컨테이너가 내려가 있을 때):

```shell
sudo chown -R $USER:$USER lv2_module5/debug
```

- 예방: `docker compose up` 전에 호스트에서 폴더를 먼저 만들어 두면 root 소유로 생성되지 않습니다.

```shell
mkdir -p lv2_module5/debug
```

  단, 컨테이너가 실행 중에 새로 만드는 파일·폴더는 다시 root 소유가 될 수 있습니다. 같은 오류가 다시 나면 위 `chown`을 다시 실행합니다.