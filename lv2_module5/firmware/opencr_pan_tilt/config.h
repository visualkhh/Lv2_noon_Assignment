#ifndef OPENCR_PAN_TILT_CONFIG_H_
#define OPENCR_PAN_TILT_CONFIG_H_

// ===== 통신 =====
#define CMD_SERIAL Serial            // USB CDC: Raspberry Pi(DynamixelController) ↔ OpenCR
#define DXL_SERIAL Serial3           // OpenCR Dynamixel TTL 포트
const int DXL_DIR_PIN = 84;          // OpenCR DXL 방향 제어 핀

const uint32_t CMD_BAUD = 115200;
const uint32_t DXL_BAUD = 57600;     // TODO(심규진): 실제 모터 baud 확인
const float DXL_PROTOCOL = 2.0;      // TODO(심규진): 실제 프로토콜 확인

// ===== 조인트 =====
// pan(좌우)이 베이스, tilt(상하)가 그 위에 붙고, tilt 링크 끝에 RealSense가 장착된다.
const uint8_t PAN_ID = 1;            // TODO(심규진): 실제 ID 확인
const uint8_t TILT_ID = 2;           // TODO(심규진): 실제 ID 확인

// 회전 범위 [deg, Present Position 기준 절대각]. 케이블·브래킷 간섭이 없는 범위로 실측 후 설정
const float PAN_MIN_DEG = 135.0f;    // TODO(심규진): 실측
const float PAN_MAX_DEG = 225.0f;    // TODO(심규진): 실측
const float TILT_MIN_DEG = 150.0f;   // TODO(심규진): 실측
const float TILT_MAX_DEG = 210.0f;   // TODO(심규진): 실측

// ===== 안전 =====
const float MAX_VEL_RAD_S = 0.5f;            // 펌웨어 측 속도 상한 [rad/s]
const unsigned long CMD_TIMEOUT_MS = 500;    // 명령 미수신 시 정지
const unsigned long LIMIT_CHECK_MS = 20;     // 회전 범위 확인 주기

#endif  // OPENCR_PAN_TILT_CONFIG_H_
