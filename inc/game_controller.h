#pragma once

#include <Arduino.h>

#include "events.h"
#include "game_engine.h"
#include "music.h"

class GameController {
public:
    void begin();
    bool handleInputEvent(const InputEvent& event);
    bool handleRemoteEvent(const RemoteEvent& event);
    bool tick(uint32_t nowMs);
    bool popOutboundPacket(NetPacket& packet);
    bool popMusicEvent(MusicEvent& event);

    void makeDisplayConfigEvent(RenderEvent& event) const;
    void makeScreenRenderEvent(RenderEvent& event) const;

private:
    static constexpr uint8_t OUTBOUND_PACKET_CAPACITY = 12;
    static constexpr uint8_t MUSIC_EVENT_CAPACITY = 8;

    struct StartRequestState {
        bool valid;
        uint8_t requestId;
        uint16_t settingsRevision;
        uint16_t settingsOwnerId;
        MatchSettings settings;
        uint32_t seed;
    };

    struct PauseActionState {
        bool valid;
        uint8_t pauseId;
        uint8_t requestId;
        PauseMenuAction action;
        uint32_t restartSeed;
    };

    struct PostGameActionState {
        bool valid;
        uint8_t requestId;
        PostGameMenuAction action;
        uint32_t restartSeed;
    };

    bool handleGameplayInput(const InputEvent& event);
    bool handleLobbyInput(const InputEvent& event);
    bool handlePausedInput(const InputEvent& event);
    bool handleGameOverInput(const InputEvent& event);

    bool handlePacket(const NetPacket& packet);
    bool handlePresencePacket(const NetPacket& packet);
    bool handleLobbySettingsPacket(const NetPacket& packet);
    bool handleStartGameRequestPacket(const NetPacket& packet);
    bool handleStartGameReplyPacket(const NetPacket& packet);
    bool handlePausePacket(const NetPacket& packet);
    bool handlePauseActionRequestPacket(const NetPacket& packet);
    bool handlePauseActionReplyPacket(const NetPacket& packet);
    bool handlePostGameActionRequestPacket(const NetPacket& packet);
    bool handlePostGameActionReplyPacket(const NetPacket& packet);
    bool handleGameOverPacket(const NetPacket& packet);
    bool handleGarbagePacket(const NetPacket& packet);

    bool handleStepResult(const GameEngine::StepResult& result);
    bool queueOutboundPacket(const NetPacket& packet);
    bool queueMusicEvent(MusicEventType type);
    void queuePresencePacket();
    void queueLobbySettingsPacket();
    void queueStartGameRequestPacket(uint32_t seed);
    void queueStartGameReplyPacket(uint8_t requestId, bool accepted);
    void queuePausePacket(uint8_t pauseId);
    void queuePauseActionRequestPacket(const PauseActionState& request);
    void queuePauseActionReplyPacket(const PauseActionState& request, bool accepted);
    void queuePostGameActionRequestPacket(const PostGameActionState& request);
    void queuePostGameActionReplyPacket(const PostGameActionState& request, bool accepted);
    void queueGarbagePacket(const GameEngine::GarbageAttack& garbage);
    void queueGameOverPacket();

    void beginGame(uint32_t seed, const MatchSettings& settings);
    void returnToLobby();
    void enterPaused(uint8_t pauseId);
    void executePauseAction(PauseMenuAction action, uint32_t restartSeed);
    void executePostGameAction(PostGameMenuAction action, uint32_t restartSeed);
    void finalizeLocalGameOver(GameOverReason reason);
    void updatePostGameState();

    bool isNavigationEvent(const InputEvent& event) const;
    int8_t navigationDelta(const InputEvent& event) const;
    bool isSelectEvent(const InputEvent& event) const;
    bool mapsToAction(PhysicalButton button, GameEngine::Action& action) const;
    bool mapsToRepeatAction(PhysicalButton button, GameEngine::Action& action) const;

    UserSettings userSettings_;
    MatchSettings matchSettings_;
    GameEngine::Engine engine_;

    GamePhase phase_;
    uint16_t localDeviceId_;
    bool transportReady_;

    bool remoteOnline_;
    uint16_t remoteDeviceId_;
    PresenceState remotePresenceState_;
    char remoteUsername_[MAX_USERNAME_LEN + 1];

    uint16_t settingsRevision_;
    uint16_t settingsOwnerId_;
    uint32_t currentSeed_;

    LobbyMenuItem lobbySelection_;
    ModalState lobbyModalState_;
    bool lobbyConfirmAcceptSelected_;
    StartRequestState pendingIncomingStart_;
    StartRequestState pendingOutgoingStart_;
    uint8_t nextStartRequestId_;

    uint8_t currentPauseId_;
    PauseMenuAction pauseSelection_;
    PausePanelState pausePanelState_;
    bool pauseConfirmAcceptSelected_;
    PauseActionState pendingIncomingPauseAction_;
    PauseActionState pendingOutgoingPauseAction_;
    uint8_t nextPauseId_;
    uint8_t nextPauseActionRequestId_;

    bool localTerminal_;
    bool remoteTerminal_;
    bool localFinishedBeforeRemote_;
    bool localWon_;
    bool disconnected_;
    GameOverReason localGameOverReason_;
    GameOverReason remoteGameOverReason_;
    uint32_t remoteFinalScore_;
    uint16_t remoteLinesCleared_;
    PostGameMenuAction postGameSelection_;
    GameOverPanelState gameOverPanelState_;
    bool gameOverConfirmAcceptSelected_;
    PostGameActionState pendingIncomingPostGameAction_;
    PostGameActionState pendingOutgoingPostGameAction_;
    uint8_t nextPostGameRequestId_;
    bool localGameOverPacketQueued_;

    NetPacket outboundPackets_[OUTBOUND_PACKET_CAPACITY];
    uint8_t outboundHead_;
    uint8_t outboundCount_;

    MusicEvent musicEvents_[MUSIC_EVENT_CAPACITY];
    uint8_t musicHead_;
    uint8_t musicCount_;
};
