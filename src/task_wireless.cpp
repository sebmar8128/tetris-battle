#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <string.h>

#include "config.h"
#include "events.h"
#include "queues.h"
#include "tasks.h"

namespace {

struct MacAddress {
    uint8_t bytes[ESPNOW_PEER_ADDR_LEN];
};

// Replace these placeholders with the two boards' actual STA MAC addresses.
constexpr MacAddress DEVICE_A_MAC = {{0xE0, 0x72, 0xA1, 0xD6, 0x3C, 0xAC}};
constexpr MacAddress DEVICE_B_MAC = {{0xE0, 0x72, 0xA1, 0xD6, 0x40, 0x7C}};

#if defined(BOARD_ROLE_A)
constexpr MacAddress SELF_MAC = DEVICE_A_MAC;
constexpr MacAddress PEER_MAC = DEVICE_B_MAC;
#elif defined(BOARD_ROLE_B)
constexpr MacAddress SELF_MAC = DEVICE_B_MAC;
constexpr MacAddress PEER_MAC = DEVICE_A_MAC;
#else
#error "Build with 'pio run -e a' or 'pio run -e b'."
#endif

struct ReceivedFrame {
    uint8_t sourceMac[ESPNOW_PEER_ADDR_LEN];
    NetPacket packet;
};

struct ReliableQueueEntry {
    bool valid;
    NetPacket packet;
};

QueueHandle_t g_wirelessRxQueue = nullptr;

bool reliablePacketType(PacketType type) {
    return type != PacketType::Presence && type != PacketType::Ack;
}

uint16_t deriveDeviceId(const uint8_t mac[ESPNOW_PEER_ADDR_LEN]) {
    const uint16_t value = (static_cast<uint16_t>(mac[4]) << 8) | mac[5];
    return value == 0 ? 1 : value;
}

bool sameMac(const uint8_t a[ESPNOW_PEER_ADDR_LEN], const uint8_t b[ESPNOW_PEER_ADDR_LEN]) {
    return memcmp(a, b, ESPNOW_PEER_ADDR_LEN) == 0;
}

void onEspNowSend(const uint8_t* macAddr, esp_now_send_status_t status) {
    (void)macAddr;
    (void)status;
}

void onEspNowReceive(const uint8_t* macAddr, const uint8_t* data, int dataLen) {
    if (g_wirelessRxQueue == nullptr || macAddr == nullptr || data == nullptr) {
        return;
    }

    if (dataLen != static_cast<int>(sizeof(NetPacket))) {
        return;
    }

    ReceivedFrame frame = {};
    memcpy(frame.sourceMac, macAddr, ESPNOW_PEER_ADDR_LEN);
    memcpy(&frame.packet, data, sizeof(NetPacket));
    (void)xQueueSend(g_wirelessRxQueue, &frame, 0);
}

bool ensurePeer(const uint8_t mac[ESPNOW_PEER_ADDR_LEN]) {
    if (esp_now_is_peer_exist(mac)) {
        return true;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, ESPNOW_PEER_ADDR_LEN);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    return esp_now_add_peer(&peerInfo) == ESP_OK;
}

bool sendPacketTo(const uint8_t mac[ESPNOW_PEER_ADDR_LEN], const NetPacket& packet) {
    return esp_now_send(mac, reinterpret_cast<const uint8_t*>(&packet), sizeof(NetPacket)) == ESP_OK;
}

void emitRemoteEvent(const RemoteEvent& event) {
    (void)xQueueSend(g_remoteEventQueue, &event, 0);
}

void emitTransportReadyEvent(uint16_t localDeviceId) {
    RemoteEvent event = {};
    event.type = RemoteEventType::TransportReady;
    event.payload.transportReady.localDeviceId = localDeviceId;
    emitRemoteEvent(event);
}

void emitPeerDisconnectedEvent(const uint8_t remoteMac[ESPNOW_PEER_ADDR_LEN]) {
    RemoteEvent event = {};
    event.type = RemoteEventType::PeerDisconnected;
    memcpy(event.peerAddress, remoteMac, ESPNOW_PEER_ADDR_LEN);
    emitRemoteEvent(event);
}

void emitPacketReceivedEvent(const uint8_t remoteMac[ESPNOW_PEER_ADDR_LEN], const NetPacket& packet) {
    RemoteEvent event = {};
    event.type = RemoteEventType::PacketReceived;
    memcpy(event.peerAddress, remoteMac, ESPNOW_PEER_ADDR_LEN);
    event.payload.packet = packet;
    emitRemoteEvent(event);
}

bool enqueueReliablePacket(
    ReliableQueueEntry pending[WIRELESS_PENDING_PACKET_CAPACITY],
    uint8_t& pendingCount,
    const NetPacket& packet
) {
    if (packet.header.type == PacketType::LobbySettings) {
        for (uint8_t i = 0; i < pendingCount; ++i) {
            if (pending[i].valid && pending[i].packet.header.type == PacketType::LobbySettings) {
                pending[i].packet = packet;
                return true;
            }
        }
    }

    if (pendingCount >= WIRELESS_PENDING_PACKET_CAPACITY) {
        return false;
    }

    pending[pendingCount].valid = true;
    pending[pendingCount].packet = packet;
    ++pendingCount;
    return true;
}

bool dequeueReliablePacket(
    ReliableQueueEntry pending[WIRELESS_PENDING_PACKET_CAPACITY],
    uint8_t& pendingCount,
    NetPacket& packet
) {
    if (pendingCount == 0) {
        return false;
    }

    packet = pending[0].packet;
    for (uint8_t i = 1; i < pendingCount; ++i) {
        pending[i - 1] = pending[i];
    }
    pending[pendingCount - 1] = {};
    --pendingCount;
    return true;
}

void stampPacket(NetPacket& packet, uint16_t localDeviceId, uint8_t seq) {
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.senderId = localDeviceId;
    packet.header.seq = seq;
}

void sendAck(const uint8_t remoteMac[ESPNOW_PEER_ADDR_LEN], uint16_t localDeviceId, uint8_t ackSeq) {
    NetPacket ack = {};
    ack.header.protocolVersion = NET_PROTOCOL_VERSION;
    ack.header.type = PacketType::Ack;
    ack.header.senderId = localDeviceId;
    ack.header.seq = 0;
    ack.payload.ack.ackSeq = ackSeq;
    (void)sendPacketTo(remoteMac, ack);
}

bool initWirelessStack() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    (void)esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() != ESP_OK) {
        return false;
    }

    if (esp_now_register_send_cb(onEspNowSend) != ESP_OK) {
        return false;
    }

    if (esp_now_register_recv_cb(onEspNowReceive) != ESP_OK) {
        return false;
    }

    return ensurePeer(PEER_MAC.bytes);
}

}  // namespace

void wirelessTask(void* pvParameters) {
    (void)pvParameters;

    g_wirelessRxQueue = xQueueCreate(
        WIRELESS_RX_FRAME_CAPACITY,
        sizeof(ReceivedFrame)
    );

    if (g_wirelessRxQueue == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    if (!initWirelessStack()) {
        vTaskDelete(nullptr);
        return;
    }

    const uint16_t localDeviceId = deriveDeviceId(SELF_MAC.bytes);
    emitTransportReadyEvent(localDeviceId);

    bool remoteOnline = false;
    uint32_t lastRemoteSeenMs = 0;

    NetPacket cachedPresence = {};
    cachedPresence.header.protocolVersion = NET_PROTOCOL_VERSION;
    cachedPresence.header.type = PacketType::Presence;
    cachedPresence.payload.presence.presenceState = PresenceState::InLobby;
    cachedPresence.payload.presence.username[0] = '\0';

    ReliableQueueEntry pending[WIRELESS_PENDING_PACKET_CAPACITY] = {};
    uint8_t pendingCount = 0;

    NetPacket inFlight = {};
    bool inFlightValid = false;
    uint32_t inFlightSentMs = 0;
    uint8_t nextSeq = 1;
    uint8_t lastAcceptedSeq = 0;
    bool hasAcceptedSeq = false;

    TickType_t lastHeartbeatTick = xTaskGetTickCount();

    while(true) {
        NetPacket outboundPacket = {};
        while (xQueueReceive(g_outboundPacketQueue, &outboundPacket, 0) == pdTRUE) {
            if (outboundPacket.header.type == PacketType::Presence) {
                cachedPresence = outboundPacket;
                continue;
            }

            (void)enqueueReliablePacket(pending, pendingCount, outboundPacket);
        }

        ReceivedFrame frame = {};
        while (xQueueReceive(g_wirelessRxQueue, &frame, 0) == pdTRUE) {
            const NetPacket& packet = frame.packet;
            if (packet.header.protocolVersion != NET_PROTOCOL_VERSION) {
                continue;
            }

            if (!sameMac(frame.sourceMac, PEER_MAC.bytes)) {
                continue;
            }

            remoteOnline = true;
            lastRemoteSeenMs = millis();

            if (packet.header.type == PacketType::Ack) {
                if (inFlightValid && packet.payload.ack.ackSeq == inFlight.header.seq) {
                    inFlightValid = false;
                }
                continue;
            }

            if (reliablePacketType(packet.header.type)) {
                if (hasAcceptedSeq && packet.header.seq == lastAcceptedSeq) {
                    sendAck(PEER_MAC.bytes, localDeviceId, packet.header.seq);
                    continue;
                }

                hasAcceptedSeq = true;
                lastAcceptedSeq = packet.header.seq;
                sendAck(PEER_MAC.bytes, localDeviceId, packet.header.seq);
            }

            emitPacketReceivedEvent(PEER_MAC.bytes, packet);
        }

        const TickType_t nowTick = xTaskGetTickCount();
        if ((nowTick - lastHeartbeatTick) >= WIRELESS_HEARTBEAT_PERIOD_TICKS) {
            lastHeartbeatTick = nowTick;
            NetPacket presence = cachedPresence;
            stampPacket(presence, localDeviceId, 0);
            (void)sendPacketTo(PEER_MAC.bytes, presence);
        }

        const uint32_t nowMs = millis();
        if (remoteOnline && (nowMs - lastRemoteSeenMs) >= WIRELESS_OFFLINE_TIMEOUT_MS) {
            remoteOnline = false;
            emitPeerDisconnectedEvent(PEER_MAC.bytes);
        }

        if (inFlightValid && (nowMs - inFlightSentMs) >= WIRELESS_ACK_TIMEOUT_MS) {
            (void)sendPacketTo(PEER_MAC.bytes, inFlight);
            inFlightSentMs = nowMs;
        }

        if (!inFlightValid) {
            NetPacket nextPacket = {};
            if (dequeueReliablePacket(pending, pendingCount, nextPacket)) {
                stampPacket(nextPacket, localDeviceId, nextSeq++);
                if (sendPacketTo(PEER_MAC.bytes, nextPacket)) {
                    inFlight = nextPacket;
                    inFlightValid = true;
                    inFlightSentMs = nowMs;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
