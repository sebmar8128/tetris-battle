#include <Arduino.h>

#include "tasks.h"

void inputTask(void* pvParameters) {
    (void)pvParameters;

    while(true) {
        // Poll buttons and emit debounced input events from here.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
