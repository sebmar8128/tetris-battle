#pragma once

#include <Arduino.h>

extern TaskHandle_t g_wirelessTaskHandle;
extern TaskHandle_t g_inputTaskHandle;
extern TaskHandle_t g_gameTaskHandle;
extern TaskHandle_t g_renderTaskHandle;
extern TaskHandle_t g_storageTaskHandle;

void wirelessTask(void* pvParameters);
void inputTask(void* pvParameters);
void gameTask(void* pvParameters);
void renderTask(void* pvParameters);
void storageTask(void* pvParameters);

bool createApplicationTasks();