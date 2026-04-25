#include <Arduino.h>

#include "queues.h"
#include "tasks.h"

void storageTask(void* pvParameters) {
    (void)pvParameters;

    StorageRequest request;

    while(true) {
        if (xQueueReceive(g_storageRequestQueue, &request, portMAX_DELAY) == pdTRUE) {
            // NVS-backed settings load/save will be handled here.
        }
    }
}
