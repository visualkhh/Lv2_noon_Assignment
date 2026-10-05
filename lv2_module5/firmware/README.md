# OpenCR Pan/Tilt 펌웨어

`opencr_pan_tilt/opencr_pan_tilt.ino`는 Pi의 `dynamixel_controller`에서 USB CDC 115200 bps로 받는 상대 각도 명령을 XM430-W350-T의 절대 목표 위치로 변환한다.

```text
Pi /dev/ttyACM0 ── 115200 bps, ASCII ──> OpenCR USB Serial
OpenCR Serial3 ── Protocol 2.0, 1,000,000 bps ──> pan ID 11 / tilt ID 12
```

입력 한 줄은 `M,<pan_delta_deg>,<tilt_delta_deg>\n`이다. 예를 들어 `M,5.0,-5.0\n`의 끝에는 실제 newline 바이트가 들어간다. 스케치는 숫자가 아니거나 무한대인 값과 형식이 다른 줄을 무시한다. 각 모터별로 `TARGET += delta`를 계산하고 범위를 제한한 뒤 `GOAL = TARGET + 180°`로 `setGoalPosition(..., UNIT_DEGREE)`을 호출한다. Pi에서는 `+180°`를 적용하지 않는다. 부팅 시 현재 모터 위치를 읽어 TARGET을 초기화하므로 첫 상대 명령에서 갑자기 180°로 이동하지 않도록 설계했다.

OpenCR 통신 watchdog은 마지막 **유효한** `M` 명령부터 `COMMAND_TIMEOUT_MS`(기본 500ms)가 지나면 한 번 작동한다. 형식 오류, 부분 수신, 빈 줄은 타이머를 갱신하지 않는다. 이 펌웨어는 위치 제어 모드(`OP_POSITION`)이므로 속도 목표 0을 쓰는 대신 두 모터의 현재 위치를 읽어 새 위치 목표로 지정하여 남은 이동을 중단한다. 이후 명령은 정지한 위치를 기준으로 상대 이동한다. 모터 위치 읽기나 목표 쓰기가 실패하면 해당 모터의 토크를 끄고 새 명령을 거부하므로 원인 점검 후 OpenCR을 재시작해야 한다. 이 오류 경로에서는 하중에 따라 축이 내려갈 수 있다. 모터가 이미 움직이는 동안에도 관성 때문에 즉시 물리 속도가 0이 되지는 않는다.

## 준비

- OpenCR 1.0, 전원, XM430-W350-T 두 대. 모터 ID를 **pan 11**, **tilt 12**로 미리 설정하고 두 모터의 baud를 **1,000,000 bps**, 프로토콜을 **2.0**으로 맞춘다.
- USB 케이블로 OpenCR을 업로드할 컴퓨터에 연결한다. 실제 장착 방향과 안전한 가동 범위를 확인한다.
- Arduino IDE, ROBOTIS OpenCR 보드 패키지, `Dynamixel2Arduino` 라이브러리가 필요하다. ROBOTIS의 [OpenCR 설치 안내](https://emanual.robotis.com/docs/en/parts/controller/opencr10/)와 [Dynamixel2Arduino 저장소](https://github.com/ROBOTIS-GIT/Dynamixel2Arduino)를 참고한다.

ROBOTIS는 OpenCR 보드 매니저가 Raspberry Pi 같은 ARM SBC의 Arduino IDE를 지원하지 않는다고 안내한다. 스케치 빌드·업로드는 지원되는 PC에서 수행하고, Pi에서는 ROS 빌드와 시리얼·모터 동작을 검증한다. [ROBOTIS OpenCR 안내](https://emanual.robotis.com/docs/en/platform/turtlebot3/opencr_setup/)

## Arduino IDE에서 설치·업로드

1. Arduino IDE를 설치하고 **Preferences → Additional Boards Manager URLs**에 다음 주소를 추가한다.

   ```text
   https://raw.githubusercontent.com/ROBOTIS-GIT/OpenCR/master/arduino/opencr_release/package_opencr_index.json
   ```

2. **Boards Manager**에서 **OpenCR by ROBOTIS**를 설치하고 **Tools → Board → OpenCR Board**를 선택한다.
3. **Library Manager**에서 **Dynamixel2Arduino**를 설치한다.
4. `opencr_pan_tilt/opencr_pan_tilt.ino`를 열고 같은 디렉터리의 `config.h`에서 `PAN_ID=11`, `TILT_ID=12`, `DXL_BAUD=1000000`을 확인한다.
5. **Tools → Port**에서 OpenCR의 USB 포트를 고른다. Linux에서는 보통 `/dev/ttyACM0`이지만 연결 환경에 따라 번호가 달라진다.
6. Arduino IDE의 **Verify**로 컴파일한 다음 **Upload**한다. 업로드 중 Pi의 controller나 다른 시리얼 프로그램은 종료한다. 완료되면 OpenCR이 재시작된다.

Linux PC에서 업로드 권한 오류가 나면 ROBOTIS의 OpenCR udev 규칙 설치 절차를 따른다. 보드 업로드와 관련된 자세한 화면 절차는 [공식 OpenCR 문서](https://emanual.robotis.com/docs/en/parts/controller/opencr10/)에 있다.

## Pi에서 연결·동작 확인

1. OpenCR을 Pi에 연결한 뒤 `ls -l /dev/ttyACM*`로 포트를 확인한다. 포트가 달라졌다면 `ros2_ws/src/dynamixel/config/dynamixel.yaml`의 `serial_port`를 수정한다.
2. Pi workspace에서 `source /opt/ros/lyrical/setup.bash`와 `source install/setup.bash`를 실행한다.
3. `ros2 run dynamixel dynamixel_controller`를 켜고 `serial connected: /dev/ttyACM0 at 115200 bps` 로그를 확인한다. 이 단계에서는 모터 명령이 전송되지 않는다.
4. 장착 범위가 확인된 상태에서 작은 상대 이동을 시험한다. 다음 예시는 pan +1°, tilt 0°에 가까운 radian 값이다.

   ```bash
   ros2 topic pub --once /motor_cmd sensor_msgs/msg/JointState \
     '{name: [pan_joint, tilt_joint], position: [0.0174533, 0.0]}'
   ```

5. 실제 움직임이 Pan 쪽 +1°인지 확인한다. 방향이 반대라면 기구 방향을 확인하고 `dynamixel/config/dynamixel.yaml`의 `pan_gain` 부호를 바꾼다. Tilt도 작은 값으로 별도 확인한다. 전체 추적 실행은 [제어 패키지 README](../ros2_ws/src/dynamixel/README.md)를 따른다.

`/dev/ttyACM0`를 열 수 있어도 펌웨어가 정상 업로드되었다거나 모터가 응답한다는 뜻은 아니다. 업로드 확인, ID·버스 설정 확인, 작은 수동 명령, 전체 추적 순서로 점검한다.

## 코드와 제한

| `config.h` 상수 | 값 | 의미 |
| --- | --- | --- |
| `PAN_ID`, `TILT_ID` | 11, 12 | XM430-W350-T ID |
| `DXL_DIR_PIN` | 84 | OpenCR DYNAMIXEL 송수신 방향 핀 |
| `DXL_BAUD` | 1000000 | OpenCR ↔ 모터 통신 속도 |
| `COMMAND_TIMEOUT_MS` | 500ms | 마지막 유효 제어 명령 후 정지까지의 시간 |
| `MIN_TARGET_DEG`, `MAX_TARGET_DEG` | −180.0°, +179.9° | 목표 범위. 실제 기구의 안전 범위를 대신하지 않음 |

모터 ping이 실패하면 스케치는 명령을 적용하지 않는다. 현재 스케치는 USB로 응답 상태를 되돌려 보내지 않으므로 Pi controller의 serial open 성공만으로 모터 통신 성공을 판정하지 않는다. Arduino 빌드·업로드 및 실제 Pan/Tilt 시험은 수행 후 결과를 기록해야 한다.
