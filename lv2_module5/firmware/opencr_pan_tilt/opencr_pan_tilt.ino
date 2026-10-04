// opencr_pan_tilt — 1축 팬틸트 bring-up 스케치 (OpenCR)
// FQBN: OpenCR:OpenCR:OpenCR
// 컴파일: arduino-cli compile --fqbn OpenCR:OpenCR:OpenCR --library Dynamixel2Arduino ./opencr_pan_tilt
// 업로드: arduino-cli upload -p /dev/ttyACM0 --fqbn OpenCR:OpenCR:OpenCR ./opencr_pan_tilt
//
// NOTE (제어 담당 확인 필요): DXL_SERIAL 포트·DXL_DIR_PIN·DXL_ID·baud는
// ROBOTIS e-Manual(OpenCR + Dynamixel2Arduino)과 대조 후 확정할 것.
// 아래 값은 bring-up용 초안이다.
#include <Dynamixel2Arduino.h>

#define DEBUG_SERIAL Serial  // USB (PC 로그·목표각 수신)
#define DXL_SERIAL Serial3   // 다이나믹셀 포트 (TODO: e-Manual 대조)
const int DXL_DIR_PIN = 22;  // TODO: e-Manual 대조

const uint8_t DXL_ID = 1;      // TODO: 실제 모터 ID로 변경
const float DXL_PROTOCOL = 2.0;
const uint32_t DXL_BAUD = 57600;  // TODO: 실제 baud로 변경

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);

void setup() {
  DEBUG_SERIAL.begin(115200);
  while (!DEBUG_SERIAL) {
  }
  dxl.begin(DXL_BAUD);
  dxl.setPortProtocolVersion(DXL_PROTOCOL);
  DEBUG_SERIAL.println("[opencr_pan_tilt] ready");
}

void loop() {
  // USB로 들어온 목표각(도)을 모터에 그대로 전달.
  // P 제어·안전 정지는 ROS 2 노드에서 담당, 여기는 전달만.
  if (DEBUG_SERIAL.available()) {
    float goal = DEBUG_SERIAL.parseFloat();
    dxl.setGoalPosition(DXL_ID, goal, UNIT_DEGREE);
    DEBUG_SERIAL.print("goal=");
    DEBUG_SERIAL.println(goal);
  }
  delay(10);
}
