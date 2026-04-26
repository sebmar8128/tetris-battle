#include <Arduino.h>

#include "display.h"
#include "queues.h"
#include "tasks.h"

void renderTask(void* pvParameters) {
    (void)pvParameters;

    Display::begin();

    RenderEvent renderEvent;

    while(true) {
        if (xQueueReceive(g_renderEventQueue, &renderEvent, portMAX_DELAY) == pdTRUE) {
            if (renderEvent.type == RenderEventType::Config) {
                Display::configure(renderEvent.payload.config);
            } else {
                Display::renderScreen(renderEvent.payload.screen);
            }
        }
    }
}
