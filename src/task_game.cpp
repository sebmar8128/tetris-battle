#include <Arduino.h>

#include "queues.h"
#include "tasks.h"

namespace {

void discardPendingGameInputs() {
    InputEvent inputEvent;
    RemoteEvent remoteEvent;
    StorageResponse storageResponse;

    while (xQueueReceive(g_inputEventQueue, &inputEvent, 0) == pdTRUE) {
    }

    while (xQueueReceive(g_remoteEventQueue, &remoteEvent, 0) == pdTRUE) {
    }

    while (xQueueReceive(g_storageResponseQueue, &storageResponse, 0) == pdTRUE) {
    }
}

}  // namespace

void gameTask(void* pvParameters) {
    (void)pvParameters;

    while(true) {
        // The game task will own match state and consume all inbound events.
        // Until that logic exists, drain the queues so placeholder producers
        // cannot fill them indefinitely.
        discardPendingGameInputs();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
