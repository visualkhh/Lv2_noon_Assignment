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
}

void loop()
{
    while (Serial.available() > 0)
    {
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
}
