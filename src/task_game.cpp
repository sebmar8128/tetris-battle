#include <Arduino.h>

#include "game_controller.h"
#include "queues.h"
#include "tasks.h"

namespace {

void discardUnusedQueues() {
    StorageResponse storageResponse;

    while (xQueueReceive(g_storageResponseQueue, &storageResponse, 0) == pdTRUE) {
    }
}

}  // namespace

void gameTask(void* pvParameters) {
    (void)pvParameters;

    GameController controller;
    controller.begin();

    RenderEvent renderEvent = {};
    controller.makeDisplayConfigEvent(renderEvent);
    (void)xQueueOverwrite(g_renderEventQueue, &renderEvent);
    vTaskDelay(pdMS_TO_TICKS(25));
    controller.makeScreenRenderEvent(renderEvent);
    (void)xQueueOverwrite(g_renderEventQueue, &renderEvent);

    TickType_t lastWakeTick = xTaskGetTickCount();

    while(true) {
        const uint32_t nowMs = millis();
        bool stateChanged = false;

        InputEvent inputEvent;
        while (xQueueReceive(g_inputEventQueue, &inputEvent, 0) == pdTRUE) {
            stateChanged = controller.handleInputEvent(inputEvent) || stateChanged;
        }

        RemoteEvent remoteEvent;
        while (xQueueReceive(g_remoteEventQueue, &remoteEvent, 0) == pdTRUE) {
            stateChanged = controller.handleRemoteEvent(remoteEvent) || stateChanged;
        }

        discardUnusedQueues();

        stateChanged = controller.tick(nowMs) || stateChanged;

        NetPacket outboundPacket;
        while (controller.popOutboundPacket(outboundPacket)) {
            (void)xQueueSend(g_outboundPacketQueue, &outboundPacket, 0);
        }

        MusicEvent musicEvent;
        while (controller.popMusicEvent(musicEvent)) {
            (void)xQueueSend(g_musicEventQueue, &musicEvent, 0);
        }

        if (stateChanged) {
            controller.makeScreenRenderEvent(renderEvent);
            (void)xQueueOverwrite(g_renderEventQueue, &renderEvent);
        }

        vTaskDelayUntil(&lastWakeTick, pdMS_TO_TICKS(5));
    }
}
