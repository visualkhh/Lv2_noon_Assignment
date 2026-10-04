# OpenCR 펌웨어 — opencr_pan_tilt

Raspberry Pi의 `DynamixelController`에서 USB 시리얼 명령을 받아 Dynamixel 2개(pan·tilt)를 속도 모드로 구동합니다.

## 기구 구성

```
        [Intel RealSense]
               │
        ┌──────┴──────┐
        │ tilt (상하) │  ID: TILT_ID
        └──────┬──────┘
        ┌──────┴──────┐
        │ pan (좌우)  │  ID: PAN_ID
        └──────┬──────┘
            [베이스]
```

- pan 조인트가 베이스에 고정되고, tilt 조인트가 pan 위에 바로 붙습니다.
- tilt 링크 끝에 Intel RealSense 카메라가 장착됩니다.
- 기본 구현(수평 1축 추적)에서는 pan만 사용하고 tilt는 속도 0을 유지합니다.

## 파일

| 파일 | 내용 |
|---|---|
| `opencr_pan_tilt/opencr_pan_tilt.ino` | 시리얼 명령 수신, 속도 명령 적용, 타임아웃·회전 범위 정지 |
| `opencr_pan_tilt/config.h` | 모터 ID·baud·프로토콜, 회전 범위, 속도 상한, 타임아웃 |

## 설정 (`config.h`)

| 항목 | 현재 값 | 비고 |
|---|---|---|
| 명령 시리얼 | USB CDC (`Serial`), 115200 | |
| Dynamixel 포트 | `Serial3`, DIR 핀 84 | OpenCR TTL 포트 |
| Dynamixel baud | 57600 | TODO: 실제 장비 확인 |
| 프로토콜 | 2.0 | TODO: 실제 장비 확인 |
| pan ID / tilt ID | 1 / 2 | TODO: 실제 장비 확인 |
| pan 범위 [deg] | 135 ~ 225 | TODO: 실측 |
| tilt 범위 [deg] | 150 ~ 210 | TODO: 실측 |
| 속도 상한 | 0.5 rad/s | |
| 명령 타임아웃 | 500 ms | |

> 모터 모델·ID·baud·프로토콜은 실제 장비에서 확인합니다. 다른 팀의 값을 복사하지 않습니다.

## 시리얼 프로토콜 (MotorSerialCommand)

줄 단위 텍스트, 줄 끝은 `\n`입니다 (`\r`, `\r\n`도 허용).

### Raspberry Pi → OpenCR

| 명령 | 형식 | 예시 | 동작 |
|---|---|---|---|
| 속도 | `V <pan> <tilt>` | `V 0.25 0.0` | 각 조인트 속도 명령 [rad/s]. 속도 상한으로 clamp |
| 정지 | `S` | `S` | 모든 조인트 속도 0 |

- 부호: 양수 = Dynamixel Present Position이 증가하는 방향
- 전송 주기: DynamixelController가 `/motor_cmd`를 받을 때마다 (약 30Hz)
- 정지 상태에서도 주기적으로 `V 0 0` 또는 `S`를 보내 타임아웃이 걸리지 않게 합니다.

### OpenCR → Raspberry Pi

| 메시지 | 의미 |
|---|---|
| `READY` | 초기화 완료 |
| `TIMEOUT` | 500 ms 동안 명령이 없어 정지함 |
| `LIMIT pan` / `LIMIT tilt` | 회전 범위 끝에서 바깥 방향 명령을 막고 정지함 |
| `ERR ping <joint>` | 초기화 시 모터 응답 없음 |
| `ERR parse` / `ERR unknown` / `ERR overflow` | 잘못된 명령 |

## 안전 동작

| 상황 | 동작 |
|---|---|
| 명령이 500 ms 이상 없음 (DynamixelController 종료·USB 단절) | 모든 조인트 정지, `TIMEOUT` 출력 |
| 회전 범위 끝에서 바깥 방향 명령 | 해당 조인트 정지, `LIMIT` 출력 |
| 속도 명령이 상한 초과 | 상한으로 clamp |

속도 모드에서는 모터가 각도 제한을 스스로 지키지 않으므로, 펌웨어가 20 ms마다 현재 위치를 확인합니다.

## 빌드·업로드

의존성: OpenCR 보드 패키지, `Dynamixel2Arduino` 라이브러리

```bash
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/ROBOTIS-GIT/OpenCR/master/arduino/opencr_release/package_opencr_index.json
arduino-cli core update-index
arduino-cli core install OpenCR:OpenCR
arduino-cli lib install Dynamixel2Arduino

arduino-cli compile --fqbn OpenCR:OpenCR:OpenCR firmware/opencr_pan_tilt
arduino-cli upload  --fqbn OpenCR:OpenCR:OpenCR -p /dev/ttyACM0 firmware/opencr_pan_tilt
```

> ⚠️ OpenCR 보드 패키지(1.5.3)의 컴파일러·업로드 도구는 x86(32비트) 호스트용만 제공됩니다.
> Raspberry Pi(arm64)에서 빌드·업로드하려면 별도 방법이 필요합니다. TODO: 라즈베리파이에서 확인한 절차를 기록합니다.
> x86_64 PC에서는 `libc6:i386`이 필요합니다.

업로드 시 시리얼 모니터나 DynamixelController가 같은 포트(`/dev/ttyACM0`)를 점유하고 있으면 안 됩니다.

## 수동 시험

모터 출력 전에 낮은 속도로 확인합니다.

```bash
# 다른 프로그램이 포트를 쓰지 않는 상태에서
screen /dev/ttyACM0 115200
V 0.1 0     # pan 천천히 회전 → 0.5초 후 TIMEOUT 출력과 함께 정지
S           # 정지
```

수동 입력은 0.5초보다 느리므로 `V` 명령 후 곧바로 타임아웃 정지가 걸립니다. 이것으로 타임아웃 동작을 확인할 수 있습니다.
