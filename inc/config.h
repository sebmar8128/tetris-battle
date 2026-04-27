#pragma once

#include <Arduino.h>

// Core 0: wireless
// Core 1: everything else
static constexpr uint32_t CORE_WIRELESS = 0;
static constexpr uint32_t CORE_APP      = 1;

// Priorities (greater value = greater priority)
static constexpr uint32_t PRIO_WIRELESS = 5;
static constexpr uint32_t PRIO_INPUT    = 5;
static constexpr uint32_t PRIO_GAME     = 4;
static constexpr uint32_t PRIO_RENDER   = 3;
static constexpr uint32_t PRIO_STORAGE  = 2;

// Stack sizes (in bytes, not words like vanilla FreeRTOS)
static constexpr uint32_t STACK_WIRELESS = (4 * 1024);
static constexpr uint32_t STACK_INPUT    = (3 * 1024);
static constexpr uint32_t STACK_GAME     = (4 * 1024);
static constexpr uint32_t STACK_RENDER   = (6 * 1024);
static constexpr uint32_t STACK_STORAGE  = (4 * 1024);

// Queue lengths
static constexpr uint32_t QLEN_INPUT_EVENTS      = 32;
static constexpr uint32_t QLEN_REMOTE_EVENTS     = 32;
static constexpr uint32_t QLEN_OUTBOUND_PACKETS  = 48;
static constexpr uint32_t QLEN_RENDER_EVENTS     = 1;
static constexpr uint32_t QLEN_STORAGE_REQUESTS  = 8;
static constexpr uint32_t QLEN_STORAGE_RESPONSES = 8;

// Task timing
static constexpr TickType_t WIRELESS_HEARTBEAT_PERIOD_TICKS = pdMS_TO_TICKS(200);
static constexpr uint32_t WIRELESS_OFFLINE_TIMEOUT_MS       = 600;
static constexpr uint32_t WIRELESS_ACK_TIMEOUT_MS           = 25;
static constexpr uint32_t WIRELESS_PENDING_PACKET_CAPACITY  = 16;
static constexpr uint32_t WIRELESS_RX_FRAME_CAPACITY        = 16;

// Input timing
static constexpr TickType_t INPUT_POLL_PERIOD_TICKS   = pdMS_TO_TICKS(5);
static constexpr uint32_t INPUT_DEBOUNCE_MS           = 25;
static constexpr uint32_t INPUT_HOLD_DETECT_MS        = 250;
static constexpr uint32_t INPUT_REPEAT_PERIOD_MS      = 40;

// Button GPIOs. Assumption is INPUT_PULLUP.
static constexpr int8_t PIN_BUTTON_LD_UP    = 47;
static constexpr int8_t PIN_BUTTON_LD_LEFT  = 21;
static constexpr int8_t PIN_BUTTON_LD_RIGHT = 35;
static constexpr int8_t PIN_BUTTON_LD_DOWN  = 48;
static constexpr int8_t PIN_BUTTON_RD_UP    = 41;
static constexpr int8_t PIN_BUTTON_RD_LEFT  = 40;
static constexpr int8_t PIN_BUTTON_RD_RIGHT = 39; // UNUSED RIGHT NOW
static constexpr int8_t PIN_BUTTON_RD_DOWN  = 42;
static constexpr int8_t PIN_BUTTON_CENTER   = 36;
