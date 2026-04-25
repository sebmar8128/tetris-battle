#include <Arduino.h>

#include "config.h"
#include "tasks.h"

TaskHandle_t g_wirelessTaskHandle = nullptr;
TaskHandle_t g_inputTaskHandle    = nullptr;
TaskHandle_t g_gameTaskHandle     = nullptr;
TaskHandle_t g_renderTaskHandle   = nullptr;
TaskHandle_t g_storageTaskHandle  = nullptr;

bool createApplicationTasks() {
    BaseType_t ok;

    ok = xTaskCreatePinnedToCore(
        wirelessTask,
        "WirelessTask",
        STACK_WIRELESS,
        nullptr,
        PRIO_WIRELESS,
        &g_wirelessTaskHandle,
        CORE_WIRELESS
    );
    if (ok != pdPASS) return false;

    ok = xTaskCreatePinnedToCore(
        inputTask,
        "InputTask",
        STACK_INPUT,
        nullptr,
        PRIO_INPUT,
        &g_inputTaskHandle,
        CORE_APP
    );
    if (ok != pdPASS) return false;

    ok = xTaskCreatePinnedToCore(
        gameTask,
        "GameTask",
        STACK_GAME,
        nullptr,
        PRIO_GAME,
        &g_gameTaskHandle,
        CORE_APP
    );
    if (ok != pdPASS) return false;

    ok = xTaskCreatePinnedToCore(
        renderTask,
        "RenderTask",
        STACK_RENDER,
        nullptr,
        PRIO_RENDER,
        &g_renderTaskHandle,
        CORE_APP
    );
    if (ok != pdPASS) return false;

    ok = xTaskCreatePinnedToCore(
        storageTask,
        "StorageTask",
        STACK_STORAGE,
        nullptr,
        PRIO_STORAGE,
        &g_storageTaskHandle,
        CORE_APP
    );
    if (ok != pdPASS) return false;

    return true;
}