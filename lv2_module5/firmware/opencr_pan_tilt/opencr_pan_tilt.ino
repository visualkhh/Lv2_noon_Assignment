// OpenCR pan-tilt 펌웨어
//
// Raspberry Pi(DynamixelController)에서 USB 시리얼로 속도 명령을 받아
// Dynamixel 2개(pan·tilt)를 속도 모드로 구동한다.
// 명령이 CMD_TIMEOUT_MS 동안 들어오지 않으면 스스로 정지한다.
//
// 프로토콜(MotorSerialCommand)은 ../README.md 참고.

#include <Dynamixel2Arduino.h>

#include "config.h"

namespace
{

struct Joint
{
  const char * name;
  uint8_t id;
  float min_deg;
  float max_deg;
  float cmd_rad_s;
};

Joint joints[] = {
  {"pan", PAN_ID, PAN_MIN_DEG, PAN_MAX_DEG, 0.0f},
  {"tilt", TILT_ID, TILT_MIN_DEG, TILT_MAX_DEG, 0.0f},
};
const size_t JOINT_COUNT = sizeof(joints) / sizeof(joints[0]);

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);

char line_buf[64];
size_t line_len = 0;
unsigned long last_cmd_ms = 0;
unsigned long last_limit_check_ms = 0;
bool timed_out = true;

float clampVelocity(float v)
{
  if (v > MAX_VEL_RAD_S) {
    return MAX_VEL_RAD_S;
  }
  if (v < -MAX_VEL_RAD_S) {
    return -MAX_VEL_RAD_S;
  }
  return v;
}

float radPerSecToRpm(float rad_s)
{
  return rad_s * 60.0f / (2.0f * PI);
}

void setJointVelocity(Joint & joint, float rad_s)
{
  joint.cmd_rad_s = rad_s;
  dxl.setGoalVelocity(joint.id, radPerSecToRpm(rad_s), UNIT_RPM);
}

void stopAll()
{
  for (size_t i = 0; i < JOINT_COUNT; ++i) {
    setJointVelocity(joints[i], 0.0f);
  }
}

// 범위 끝에서 바깥 방향으로 향하는 명령이면 정지
bool isOutward(const Joint & joint, float rad_s)
{
  float pos_deg = dxl.getPresentPosition(joint.id, UNIT_DEGREE);
  return (pos_deg >= joint.max_deg && rad_s > 0.0f) ||
         (pos_deg <= joint.min_deg && rad_s < 0.0f);
}

void applyCommand(float pan_rad_s, float tilt_rad_s)
{
  float targets[] = {clampVelocity(pan_rad_s), clampVelocity(tilt_rad_s)};
  for (size_t i = 0; i < JOINT_COUNT; ++i) {
    if (isOutward(joints[i], targets[i])) {
      targets[i] = 0.0f;
      CMD_SERIAL.print("LIMIT ");
      CMD_SERIAL.println(joints[i].name);
    }
    setJointVelocity(joints[i], targets[i]);
  }
}

// "V <pan> <tilt>" 또는 "S"
void handleLine(char * line)
{
  if (line[0] == 'S' && line[1] == '\0') {
    stopAll();
    last_cmd_ms = millis();
    timed_out = false;
    return;
  }

  if (line[0] == 'V' && line[1] == ' ') {
    char * end = nullptr;
    float pan = strtof(line + 2, &end);
    if (end == line + 2) {
      CMD_SERIAL.println("ERR parse");
      return;
    }
    char * tilt_start = end;
    float tilt = strtof(tilt_start, &end);
    if (end == tilt_start) {
      CMD_SERIAL.println("ERR parse");
      return;
    }
    applyCommand(pan, tilt);
    last_cmd_ms = millis();
    timed_out = false;
    return;
  }

  CMD_SERIAL.println("ERR unknown");
}

void readCommand()
{
  while (CMD_SERIAL.available() > 0) {
    char c = static_cast<char>(CMD_SERIAL.read());
    if (c == '\r' || c == '\n') {
      line_buf[line_len] = '\0';
      if (line_len > 0) {
        handleLine(line_buf);
      }
      line_len = 0;
      continue;
    }
    if (line_len < sizeof(line_buf) - 1) {
      line_buf[line_len++] = c;
    } else {
      line_len = 0;  // 너무 긴 줄은 버림
      CMD_SERIAL.println("ERR overflow");
    }
  }
}

void checkTimeout()
{
  if (!timed_out && millis() - last_cmd_ms > CMD_TIMEOUT_MS) {
    stopAll();
    timed_out = true;
    CMD_SERIAL.println("TIMEOUT");
  }
}

// 속도 모드는 모터가 각도 제한을 지키지 않으므로 움직이는 동안 주기적으로 확인
void checkLimits()
{
  if (millis() - last_limit_check_ms < LIMIT_CHECK_MS) {
    return;
  }
  last_limit_check_ms = millis();

  for (size_t i = 0; i < JOINT_COUNT; ++i) {
    if (joints[i].cmd_rad_s != 0.0f && isOutward(joints[i], joints[i].cmd_rad_s)) {
      setJointVelocity(joints[i], 0.0f);
      CMD_SERIAL.print("LIMIT ");
      CMD_SERIAL.println(joints[i].name);
    }
  }
}

void setupJoint(const Joint & joint)
{
  if (!dxl.ping(joint.id)) {
    CMD_SERIAL.print("ERR ping ");
    CMD_SERIAL.println(joint.name);
    return;
  }
  dxl.torqueOff(joint.id);
  dxl.setOperatingMode(joint.id, OP_VELOCITY);
  dxl.torqueOn(joint.id);
}

}  // namespace

void setup()
{
  CMD_SERIAL.begin(CMD_BAUD);

  dxl.begin(DXL_BAUD);
  dxl.setPortProtocolVersion(DXL_PROTOCOL);

  for (size_t i = 0; i < JOINT_COUNT; ++i) {
    setupJoint(joints[i]);
  }
  stopAll();

  CMD_SERIAL.println("READY");
}

void loop()
{
  readCommand();
  checkTimeout();
  checkLimits();
}
