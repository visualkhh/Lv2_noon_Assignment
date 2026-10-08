# config — 사용한 설정

PDF: "설정이 패키지 안에 있으면 중복 복사하지 않고 경로를 연결합니다." 아래가 실제로 쓰는 설정 파일입니다.
시험에 쓴 설정을 바꿔 돌렸다면 그 사본을 `config/<패키지>_<설명>.yaml`로 남기고 보고서에서 링크합니다.

| 영역 | 파일 | 주요 값 |
|---|---|---|
| 인지 (HSV·면적·필터·QoS) | [../ros2_ws/src/realsense/config/realsense.yaml](../ros2_ws/src/realsense/config/realsense.yaml) | hsv_lower/upper, hsv_bright_*, min/max_area_ratio, aspect_max, fill_min, image_reliable |
| 카메라 (해상도·FPS) | [../ros2_ws/src/realsense/launch/realsense.launch.py](../ros2_ws/src/realsense/launch/realsense.launch.py) | color_profile 424x240x30 |
| 제어 (게인·제한·타임아웃) | [../ros2_ws/src/dynamixel/config/dynamixel.yaml](../ros2_ws/src/dynamixel/config/dynamixel.yaml) | pan/tilt_gain, deadband, max_*_command, lost_timeout, serial_port, baud_rate |
| 장치 (모터 ID·baud·범위) | [../firmware/opencr_pan_tilt/config.h](../firmware/opencr_pan_tilt/config.h) | PAN_ID 11, TILT_ID 12, DXL_BAUD 1000000, MIN/MAX_TARGET_DEG |
| 문제 3 pan 비교 A/B | [A](../ros2_ws/src/dynamixel/config/kp_pan_a.yaml) · [B](../ros2_ws/src/dynamixel/config/kp_pan_b.yaml) · [시험 절차](../문제/문제3/README.md#pan축-p-이득-비교-시험-조건-실험-전-확정) | Kp 0.015/0.030, tilt 0, 프레임당 1° 상한, 0.8m, 좌우 15cm |
