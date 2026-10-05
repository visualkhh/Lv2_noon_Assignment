docker
---

# docker start
```shell
cd docker
docker compose build
docker compose up -d  
# 또는 한번에 docker compose up -d --build

docker ps
docker compose exec lyrical test-fake_camera_bringup 0 0.1
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
source ./.venv/bin/activate
python run-controller.py
```