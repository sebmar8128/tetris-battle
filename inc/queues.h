#pragma once

#include <Arduino.h>
#include "events.h"

extern QueueHandle_t g_inputEventQueue;
extern QueueHandle_t g_remoteEventQueue;
extern QueueHandle_t g_outboundMessageQueue;
extern QueueHandle_t g_renderCommandQueue;
extern QueueHandle_t g_storageRequestQueue;
extern QueueHandle_t g_storageResponseQueue;

bool initQueues();