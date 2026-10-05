#pragma once

#include <stddef.h>
#include <stdint.h>

// XM430-W350-T IDs specified by the project contract.
constexpr uint8_t PAN_ID = 11;
constexpr uint8_t TILT_ID = 12;

constexpr int DXL_DIR_PIN = 84;
constexpr float DXL_PROTOCOL_VERSION = 2.0f;
constexpr uint32_t DXL_BAUD = 1000000;

constexpr float MIN_TARGET_DEG = -180.0f;
constexpr float MAX_TARGET_DEG = 179.9f;
constexpr size_t LINE_CAPACITY = 64;
