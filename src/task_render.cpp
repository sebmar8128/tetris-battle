#include <Arduino.h>

#include "config.h"
#include "queues.h"
#include "tasks.h"

void renderTask(void* pvParameters) {
    (void)pvParameters;

    RenderEvent renderEvent;
    TickType_t lastRenderTick = xTaskGetTickCount();

    while(true) {
        if (xQueueReceive(g_renderEventQueue, &renderEvent, portMAX_DELAY) == pdTRUE) {
            const TickType_t now = xTaskGetTickCount();
            const TickType_t elapsed = now - lastRenderTick;

            if (elapsed < RENDER_FRAME_PERIOD_TICKS) {
                vTaskDelay(RENDER_FRAME_PERIOD_TICKS - elapsed);
            }

            // Push the latest snapshot to the LCD here.
            lastRenderTick = xTaskGetTickCount();
        }
    }
}