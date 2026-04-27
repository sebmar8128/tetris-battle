#pragma once

#include <Arduino.h>
#include "events.h"
#include "music.h"

extern QueueHandle_t g_inputEventQueue;
extern QueueHandle_t g_remoteEventQueue;
extern QueueHandle_t g_outboundPacketQueue;
extern QueueHandle_t g_renderEventQueue;
extern QueueHandle_t g_storageRequestQueue;
extern QueueHandle_t g_storageResponseQueue;
extern QueueHandle_t g_musicEventQueue;

bool initQueues();
