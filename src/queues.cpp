#include "queues.h"
#include "config.h"

QueueHandle_t g_inputEventQueue       = nullptr;
QueueHandle_t g_remoteEventQueue      = nullptr;
QueueHandle_t g_outboundMessageQueue  = nullptr;
QueueHandle_t g_renderCommandQueue    = nullptr;
QueueHandle_t g_storageRequestQueue   = nullptr;
QueueHandle_t g_storageResponseQueue  = nullptr;

bool initQueues() {
    g_inputEventQueue = xQueueCreate(
        QLEN_INPUT_EVENTS,
        sizeof(InputEvent)
    );

    g_remoteEventQueue = xQueueCreate(
        QLEN_REMOTE_EVENTS,
        sizeof(RemoteEvent)
    );

    g_outboundMessageQueue = xQueueCreate(
        QLEN_OUTBOUND_MESSAGES,
        sizeof(OutboundMessage)
    );

    g_renderCommandQueue = xQueueCreate(
        QLEN_RENDER_COMMANDS,
        sizeof(RenderCommand)
    );

    g_storageRequestQueue = xQueueCreate(
        QLEN_STORAGE_REQUESTS,
        sizeof(StorageRequest)
    );

    g_storageResponseQueue = xQueueCreate(
        QLEN_STORAGE_RESPONSES,
        sizeof(StorageResponse)
    );

    return g_inputEventQueue &&
           g_remoteEventQueue &&
           g_outboundMessageQueue &&
           g_renderCommandQueue &&
           g_storageRequestQueue &&
           g_storageResponseQueue;
}