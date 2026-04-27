#include "queues.h"
#include "config.h"

QueueHandle_t g_inputEventQueue       = nullptr;
QueueHandle_t g_remoteEventQueue      = nullptr;
QueueHandle_t g_outboundPacketQueue   = nullptr;
QueueHandle_t g_renderEventQueue      = nullptr;
QueueHandle_t g_storageRequestQueue   = nullptr;
QueueHandle_t g_storageResponseQueue  = nullptr;
QueueHandle_t g_musicEventQueue       = nullptr;

bool initQueues() {
    g_inputEventQueue = xQueueCreate(
        QLEN_INPUT_EVENTS,
        sizeof(InputEvent)
    );

    g_remoteEventQueue = xQueueCreate(
        QLEN_REMOTE_EVENTS,
        sizeof(RemoteEvent)
    );

    g_outboundPacketQueue = xQueueCreate(
        QLEN_OUTBOUND_PACKETS,
        sizeof(NetPacket)
    );

    g_renderEventQueue = xQueueCreate(
        QLEN_RENDER_EVENTS,
        sizeof(RenderEvent)
    );

    g_storageRequestQueue = xQueueCreate(
        QLEN_STORAGE_REQUESTS,
        sizeof(StorageRequest)
    );

    g_storageResponseQueue = xQueueCreate(
        QLEN_STORAGE_RESPONSES,
        sizeof(StorageResponse)
    );

    g_musicEventQueue = xQueueCreate(
        QLEN_MUSIC_EVENTS,
        sizeof(MusicEvent)
    );

    return g_inputEventQueue &&
           g_remoteEventQueue &&
           g_outboundPacketQueue &&
           g_renderEventQueue &&
           g_storageRequestQueue &&
           g_storageResponseQueue &&
           g_musicEventQueue;
}
