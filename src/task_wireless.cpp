#include <Arduino.h>

#include "config.h"
#include "queues.h"
#include "tasks.h"

void wirelessTask(void* pvParameters) {
    (void)pvParameters;

    NetPacket outboundPacket;
    TickType_t lastHeartbeatTick = xTaskGetTickCount();

    while(true) {
        (void)xQueueReceive(
            g_outboundPacketQueue,
            &outboundPacket,
            WIRELESS_HEARTBEAT_PERIOD_TICKS
        );

        // Outbound ESP-NOW transmission and peer liveness checks will be added here.
        const TickType_t now = xTaskGetTickCount();
        if ((now - lastHeartbeatTick) >= WIRELESS_HEARTBEAT_PERIOD_TICKS) {
            lastHeartbeatTick = now;
        }
    }
}
