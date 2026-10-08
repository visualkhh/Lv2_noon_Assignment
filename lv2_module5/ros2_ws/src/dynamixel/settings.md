# Raspberry Pi Device Permission & OpenCR udev Setup

본 프로젝트에서는 Raspberry Pi에서 다음 장치들을 사용합니다.

- OpenCR: USB Serial (`/dev/ttyACM*`)
- Camera / RealSense: Video Device (`/dev/video*`)

Linux에서는 USB 장치를 다시 연결하면 `/dev/ttyACM0`, `/dev/ttyACM1`과 같이 장치 번호가 변경될 수 있습니다.

예를 들어:

```text
첫 번째 연결
OpenCR -> /dev/ttyACM0

재연결 후
OpenCR -> /dev/ttyACM1
```

따라서 본 프로젝트에서는 다음 두 가지 설정을 사용합니다.

1. 사용자 계정을 `dialout`, `video` 그룹에 추가
2. udev rule을 사용하여 OpenCR을 항상 `/dev/opencr`로 접근

---

## 1. 사용자 권한 설정

Serial 장치와 Camera 장치를 `sudo` 없이 사용하기 위해 현재 사용자를 `dialout`, `video` 그룹에 추가합니다.

```bash
sudo usermod -aG dialout,video $USER
```

설정 적용을 위해 Raspberry Pi를 재부팅합니다.

```bash
sudo reboot
```

재부팅 후 다음 명령어로 그룹 설정을 확인합니다.

```bash
groups
```

출력에 다음 두 그룹이 포함되어 있어야 합니다.

```text
dialout
video
```

예:

```text
sim sudo dialout video
```

### dialout

`/dev/ttyACM*`, `/dev/ttyUSB*`와 같은 Serial Device에 접근하기 위한 그룹입니다.

OpenCR은 일반적으로 다음과 같은 권한으로 생성됩니다.

```bash
ls -l /dev/ttyACM*
```

예:

```text
crw-rw---- 1 root dialout ... /dev/ttyACM0
```

따라서 사용자가 `dialout` 그룹에 포함되어 있으면 `sudo chmod 777` 없이 OpenCR을 사용할 수 있습니다.

### video

`/dev/video*`와 같은 Camera Device에 접근하기 위한 그룹입니다.

확인:

```bash
ls -l /dev/video*
```

일반적으로 다음과 같이 `video` 그룹으로 설정됩니다.

```text
crw-rw---- 1 root video ... /dev/video0
```

---

# 2. OpenCR 장치 확인

OpenCR을 Raspberry Pi에 연결한 후 다음 명령어를 실행합니다.

```bash
ls /dev/ttyACM*
```

예:

```text
/dev/ttyACM0
```

또는:

```text
/dev/ttyACM1
```

현재 OpenCR 장치가 `/dev/ttyACM0`이라고 가정하면 다음 명령어로 USB 정보를 확인합니다.

```bash
udevadm info --query=property --name=/dev/ttyACM0 | grep -E 'ID_VENDOR_ID|ID_MODEL_ID|ID_SERIAL'
```

OpenCR이 `/dev/ttyACM1`이라면:

```bash
udevadm info --query=property --name=/dev/ttyACM1 | grep -E 'ID_VENDOR_ID|ID_MODEL_ID|ID_SERIAL'
```

출력 예:

```text
ID_VENDOR_ID=xxxx
ID_MODEL_ID=yyyy
ID_SERIAL=...
```

여기서 `ID_VENDOR_ID`, `ID_MODEL_ID` 값을 확인합니다.

> 주의: README에 적힌 예제 값을 그대로 사용하지 말고 실제 OpenCR에서 확인한 값을 기준으로 udev rule을 작성하거나 검증해야 합니다.

---

# 3. udev Rule

프로젝트 repository에는 OpenCR용 udev rule 예제를 포함합니다. 실제 OpenCR에서 확인한 `idVendor`와 `idProduct`로 예제의 `xxxx`, `yyyy`를 바꿔 사용합니다.

권장 디렉터리 구조:

```text
project_root/
├── README.md
├── config/
│   └── udev/
│       └── 99-opencr.rules
├── src/
└── ...
```

`config/udev/99-opencr.rules` 예시:

```text
SUBSYSTEM=="tty", ATTRS{idVendor}=="xxxx", ATTRS{idProduct}=="yyyy", GROUP="dialout", MODE="0660", SYMLINK+="opencr"
```

각 항목의 의미는 다음과 같습니다.

```text
SUBSYSTEM=="tty"
```

Serial Device에 적용합니다.

```text
ATTRS{idVendor}=="xxxx"
ATTRS{idProduct}=="yyyy"
```

특정 USB 장치를 식별합니다.

```text
GROUP="dialout"
```

장치의 그룹을 `dialout`으로 설정합니다.

```text
MODE="0660"
```

권한을 다음과 같이 설정합니다.

```text
rw-rw----
```

즉:

```text
owner : read/write
group : read/write
others: no permission
```

마지막으로:

```text
SYMLINK+="opencr"
```

OpenCR 장치에 다음 이름을 추가합니다.

```text
/dev/opencr
```

---

# 4. udev Rule 설치

Repository를 clone한 후 프로젝트 root에서 다음 명령어를 실행합니다.

```bash
sudo cp config/udev/99-opencr.rules /etc/udev/rules.d/
```

udev rule을 다시 불러옵니다.

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

그다음 OpenCR USB를 한 번 분리한 후 다시 연결합니다.

---

# 5. `/dev/opencr` 확인

다음 명령어를 실행합니다.

```bash
ls -l /dev/opencr
```

정상적으로 적용되었다면 다음과 비슷하게 출력됩니다.

```text
/dev/opencr -> ttyACM0
```

또는:

```text
/dev/opencr -> ttyACM1
```

실제 Linux 장치 번호가 변경되더라도:

```text
/dev/ttyACM0
/dev/ttyACM1
```

ROS 2에서는 항상 다음 장치를 사용할 수 있습니다.

```text
/dev/opencr
```

---

# 6. ROS 2 설정

본 프로젝트에서는 `/dev/ttyACM0`을 직접 사용하지 않습니다.

잘못된 예:

```yaml
serial_port: /dev/ttyACM0
```

권장 설정:

```yaml
serial_port: /dev/opencr
```

예를 들어 `dynamixel.yaml`:

```yaml
dynamixel_controller:
  ros__parameters:
    serial_port: /dev/opencr
```

이렇게 하면 OpenCR이 실제로 `/dev/ttyACM0` 또는 `/dev/ttyACM1` 중 어느 장치 번호를 받아도 동일한 설정으로 사용할 수 있습니다.

---

# 7. 최종 구조

장치 접근 흐름은 다음과 같습니다.

```text
Raspberry Pi User
        │
        ├── dialout group
        │       │
        │       └── OpenCR Serial 접근
        │
        └── video group
                │
                └── Camera 접근


OpenCR
   │
   ▼
Linux USB Device
   │
   ├── /dev/ttyACM0
   │        or
   └── /dev/ttyACM1
            │
            ▼
       udev rule
            │
            ▼
       /dev/opencr
            │
            ▼
DynamixelController
```

---

# 8. Troubleshooting

## `/dev/opencr`이 생성되지 않는 경우

현재 OpenCR 장치를 확인합니다.

```bash
ls /dev/ttyACM*
```

udev 정보를 확인합니다.

```bash
udevadm info --query=property --name=/dev/ttyACM0
```

또는:

```bash
udevadm info --query=property --name=/dev/ttyACM1
```

`idVendor`, `idProduct`가 `99-opencr.rules`의 값과 동일한지 확인합니다.

rule을 다시 불러옵니다.

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

OpenCR을 다시 연결합니다.

---

## Permission denied가 발생하는 경우

현재 사용자의 그룹을 확인합니다.

```bash
groups
```

`dialout`이 없다면:

```bash
sudo usermod -aG dialout $USER
```

Camera 접근 문제라면:

```bash
sudo usermod -aG video $USER
```

설정 후 반드시 로그아웃/로그인하거나 재부팅합니다.

```bash
sudo reboot
```

---

## 임시로 `chmod 777`을 사용하지 않는 이유

다음과 같은 방법은 권장하지 않습니다.

```bash
sudo chmod 777 /dev/ttyACM0
```

또는:

```bash
sudo chmod 777 /dev/video0
```

USB 장치를 분리하면 해당 `/dev` 장치 파일은 삭제되고, 재연결 시 새로운 장치 파일이 만들어지므로 기존 `chmod` 설정이 유지되지 않습니다.

또한 `777`은 모든 사용자에게 불필요한 권한까지 부여합니다.

따라서 본 프로젝트에서는:

```text
dialout / video 그룹
+
udev rule
```

방식을 사용합니다.
