#pragma once

#include <Arduino.h>

extern TaskHandle_t g_wirelessTaskHandle;
extern TaskHandle_t g_inputTaskHandle;
extern TaskHandle_t g_gameTaskHandle;
extern TaskHandle_t g_renderTaskHandle;
extern TaskHandle_t g_storageTaskHandle;
extern TaskHandle_t g_musicTaskHandle;

void wirelessTask(void* pvParameters);
void inputTask(void* pvParameters);
void gameTask(void* pvParameters);
void renderTask(void* pvParameters);
void storageTask(void* pvParameters);
void musicTask(void* pvParameters);

bool createApplicationTasks();
