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
static constexpr uint32_t QLEN_OUTBOUND_MESSAGES = 32; 
static constexpr uint32_t QLEN_RENDER_COMMANDS   = 32;
static constexpr uint32_t QLEN_STORAGE_REQUESTS  = 8;
static constexpr uint32_t QLEN_STORAGE_RESPONSES = 8;

// Timing (frame rendering roughly 60 Hz)
static constexpr TickType_t WIRELESS_HEARTBEAT_PERIOD_TICKS = pdMS_TO_TICKS(200);
static constexpr TickType_t RENDER_FRAME_PERIOD_TICKS       = pdMS_TO_TICKS(16);



