#include <Dynamixel2Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

Dynamixel2Arduino dxl(Serial3, DXL_DIR_PIN);
float pan_target_deg = 0.0f;
float tilt_target_deg = 0.0f;
char line[LINE_CAPACITY];
size_t line_length = 0;
bool motors_ready = false;
uint32_t last_command_ms = 0;
bool watchdog_tripped = false;
uint32_t last_state_publish_ms = 0;
constexpr uint32_t STATE_PUBLISH_PERIOD_MS = 50;

void stop_at_present_position(uint8_t id, float &target_deg)
{
    const float position = dxl.getPresentPosition(id, UNIT_DEGREE);
    if (!isfinite(position) || dxl.getLastLibErrCode() != 0 ||
        !dxl.setGoalPosition(id, position, UNIT_DEGREE))
    {
        // A position hold cannot be established without a valid bus response.
        dxl.torqueOff(id);
        motors_ready = false;
        return;
    }
    target_deg = constrain(position - 180.0f, MIN_TARGET_DEG, MAX_TARGET_DEG);
}

void publish_motor_state()
{
    const float pan_position_deg = dxl.getPresentPosition(PAN_ID, UNIT_DEGREE);
    if (!isfinite(pan_position_deg) || dxl.getLastLibErrCode() != 0)
        return;
    const float pan_velocity_rpm = dxl.getPresentVelocity(PAN_ID, UNIT_RPM);
    if (!isfinite(pan_velocity_rpm) || dxl.getLastLibErrCode() != 0)
        return;
    const float tilt_position_deg = dxl.getPresentPosition(TILT_ID, UNIT_DEGREE);
    if (!isfinite(tilt_position_deg) || dxl.getLastLibErrCode() != 0)
        return;
    const float tilt_velocity_rpm = dxl.getPresentVelocity(TILT_ID, UNIT_RPM);
    if (!isfinite(tilt_velocity_rpm) || dxl.getLastLibErrCode() != 0)
        return;

    // Report position relative to the project's 180 degree center; velocity is RPM.
    Serial.print("S,");
    Serial.print(pan_position_deg - 180.0f, 4);
    Serial.print(',');
    Serial.print(pan_velocity_rpm, 4);
    Serial.print(',');
    Serial.print(tilt_position_deg - 180.0f, 4);
    Serial.print(',');
    Serial.println(tilt_velocity_rpm, 4);
}

void check_command_timeout()
{
    if (!motors_ready || watchdog_tripped ||
        static_cast<uint32_t>(millis() - last_command_ms) < COMMAND_TIMEOUT_MS)
        return;

    // Position mode has no velocity goal: hold both motors where they are now.
    stop_at_present_position(PAN_ID, pan_target_deg);
    stop_at_present_position(TILT_ID, tilt_target_deg);
    watchdog_tripped = true;
}

bool parse_delta(char *text, float &value)
{
    char *end = nullptr;
    value = strtof(text, &end);
    return end != text && *end == '\0' && isfinite(value);
}

void apply_line()
{
    if (!motors_ready)
        return;
    if (strncmp(line, "M,", 2) != 0)
        return;
    char *comma = strchr(line + 2, ',');
    if (comma == nullptr)
        return;
    *comma = '\0';
    float pan_delta, tilt_delta;
    if (!parse_delta(line + 2, pan_delta) || !parse_delta(comma + 1, tilt_delta))
        return;

    // TARGET is relative to the motor's 180 degree center. Only OpenCR adds 180.
    pan_target_deg = constrain(pan_target_deg + pan_delta, MIN_TARGET_DEG, MAX_TARGET_DEG);
    tilt_target_deg = constrain(tilt_target_deg + tilt_delta, MIN_TARGET_DEG, MAX_TARGET_DEG);
    dxl.setGoalPosition(PAN_ID, pan_target_deg + 180.0f, UNIT_DEGREE);
    dxl.setGoalPosition(TILT_ID, tilt_target_deg + 180.0f, UNIT_DEGREE);
    last_command_ms = millis();
    watchdog_tripped = false;
}

void setup()
{
    Serial.begin(115200);
    dxl.begin(DXL_BAUD);
    dxl.setPortProtocolVersion(DXL_PROTOCOL_VERSION);
    if (!dxl.ping(PAN_ID) || !dxl.ping(TILT_ID))
        return;
    dxl.torqueOff(PAN_ID);
    dxl.torqueOff(TILT_ID);
    dxl.setOperatingMode(PAN_ID, OP_POSITION);
    dxl.setOperatingMode(TILT_ID, OP_POSITION);
    // Start at the actual position; the first relative command must not jump to center.
    pan_target_deg = constrain(dxl.getPresentPosition(PAN_ID, UNIT_DEGREE) - 180.0f, MIN_TARGET_DEG, MAX_TARGET_DEG);
    tilt_target_deg = constrain(dxl.getPresentPosition(TILT_ID, UNIT_DEGREE) - 180.0f, MIN_TARGET_DEG, MAX_TARGET_DEG);
    dxl.torqueOn(PAN_ID);
    dxl.torqueOn(TILT_ID);
    motors_ready = true;
    last_command_ms = millis();
}

void loop()
{
    check_command_timeout();
    while (Serial.available() > 0)
    {
        check_command_timeout();
        const char ch = static_cast<char>(Serial.read());
        if (ch == '\n')
        {
            line[line_length] = '\0';
            apply_line();
            line_length = 0;
        }
        else if (ch != '\r' && line_length < LINE_CAPACITY - 1)
        {
            line[line_length++] = ch;
        }
        else if (line_length >= LINE_CAPACITY - 1)
        {
            line_length = 0;
        }
    }

    const uint32_t now_ms = millis();
    if (motors_ready &&
        static_cast<uint32_t>(now_ms - last_state_publish_ms) >= STATE_PUBLISH_PERIOD_MS)
    {
        last_state_publish_ms = now_ms;
        publish_motor_state();
    }
}
