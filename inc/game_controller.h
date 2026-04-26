#pragma once

#include <Arduino.h>

#include "events.h"
#include "game_engine.h"

class GameController {
public:
    void begin();
    bool handleInputEvent(const InputEvent& event);
    bool handleRemoteEvent(const RemoteEvent& event);
    bool tick(uint32_t nowMs);
    bool popOutboundPacket(NetPacket& packet);

    bool isGameOver() const;

    void makeDisplayConfigEvent(RenderEvent& event) const;
    void makeGameplayRenderEvent(RenderEvent& event) const;
    void makeGameOverRenderEvent(RenderEvent& event) const;

private:
    static constexpr uint8_t OUTBOUND_PACKET_CAPACITY = 8;

    bool handleStepResult(const GameEngine::StepResult& result);
    bool queueOutboundPacket(const NetPacket& packet);
    void queueGarbagePacket(const GameEngine::GarbageAttack& garbage);
    void queueGameOverPacket();
    bool mapsToAction(PhysicalButton button, GameEngine::Action& action) const;
    bool mapsToRepeatAction(PhysicalButton button, GameEngine::Action& action) const;

    UserSettings userSettings_;
    MatchSettings matchSettings_;
    GameEngine::Engine engine_;
    NetPacket outboundPackets_[OUTBOUND_PACKET_CAPACITY];
    uint8_t outboundHead_;
    uint8_t outboundCount_;
    bool localGameOverPacketQueued_;
};
