#pragma once

#include <Arduino.h>

#include "types.h"

// InputTask -> GameTask

enum class InputEventType : uint8_t {
    Press,
    Release,
    Repeat
};

struct InputEvent {
    PhysicalButton button;
    InputEventType type;
    uint32_t tickMs;
};


// WirelessTask <-> GameTask

static constexpr uint8_t NET_PROTOCOL_VERSION = 1;

enum class PacketType : uint8_t {
    Presence,
    Ack,
    LobbySettings,
    StartGameRequest,
    StartGameReply,
    Pause,
    PauseActionRequest,
    PauseActionReply,
    PostGameActionRequest,
    PostGameActionReply,
    Garbage,
    GameOver
};

struct NetHeader {
    uint8_t protocolVersion;
    PacketType type;
    uint16_t senderId;
    uint8_t seq;
};

struct PresencePacket {
    PresenceState presenceState;
    char username[MAX_USERNAME_LEN + 1];
};

struct AckPacket {
    uint8_t ackSeq;
};

struct LobbySettingsPacket {
    uint16_t revision;
    MatchSettings settings;
};

struct StartGameRequestPacket {
    uint8_t requestId;
    uint16_t settingsRevision;
    uint16_t settingsOwnerId;
    MatchSettings settings;
    uint32_t seed;
};

struct StartGameReplyPacket {
    uint8_t requestId;
    bool accepted;
};

struct PausePacket {
    uint8_t pauseId;
};

struct PauseActionRequestPacket {
    uint8_t pauseId;
    uint8_t requestId;
    PauseMenuAction action;
    uint32_t restartSeed;
};

struct PauseActionReplyPacket {
    uint8_t pauseId;
    uint8_t requestId;
    PauseMenuAction action;
    bool accepted;
};

struct PostGameActionRequestPacket {
    uint8_t requestId;
    PostGameMenuAction action;
    uint32_t restartSeed;
};

struct PostGameActionReplyPacket {
    uint8_t requestId;
    PostGameMenuAction action;
    bool accepted;
};

struct GarbagePacket {
    uint8_t lines;
    uint8_t holeColumn;
};

struct GameOverPacket {
    GameOverReason reason;
    uint32_t finalScore;
    uint16_t linesCleared;
};

struct NetPacket {
    NetHeader header;
    union {
        PresencePacket presence;
        AckPacket ack;
        LobbySettingsPacket lobbySettings;
        StartGameRequestPacket startGameRequest;
        StartGameReplyPacket startGameReply;
        PausePacket pause;
        PauseActionRequestPacket pauseActionRequest;
        PauseActionReplyPacket pauseActionReply;
        PostGameActionRequestPacket postGameActionRequest;
        PostGameActionReplyPacket postGameActionReply;
        GarbagePacket garbage;
        GameOverPacket gameOver;
    } payload;
};

enum class RemoteEventType : uint8_t {
    TransportReady,
    PacketReceived,
    PeerDisconnected
};

struct TransportReadyEvent {
    uint16_t localDeviceId;
};

struct RemoteEvent {
    RemoteEventType type;
    uint8_t peerAddress[ESPNOW_PEER_ADDR_LEN];
    union {
        TransportReadyEvent transportReady;
        NetPacket packet;
    } payload;
};


// GameTask -> RenderTask

enum class RenderScreen : uint8_t {
    Lobby,
    Gameplay,
    Paused,
    GameOver
};

enum class LobbyMenuItem : uint8_t {
    GarbageEnabled,
    Mode,
    StartingLevel,
    MusicEnabled,
    StartGame
};

enum class PausePanelState : uint8_t {
    Menu,
    OutgoingRequest,
    IncomingRequest
};

enum class GameOverPanelState : uint8_t {
    WaitingForRemote,
    Menu,
    OutgoingRequest,
    IncomingRequest,
    Disconnected
};

enum class RenderEventType : uint8_t {
    Config,
    Screen
};

struct DisplayRenderConfig {
    ThemeId theme;
    bool holdEnabled;
    bool ghostEnabled;
    uint8_t nextPreviewCount;
};

struct LobbyRenderState {
    char localUsername[MAX_USERNAME_LEN + 1];
    char remoteUsername[MAX_USERNAME_LEN + 1];
    LobbyPeerStatus remoteStatus;
    MatchSettings matchSettings;
    uint16_t revision;
    LobbyMenuItem selectedItem;
    ModalState modalState;
    bool incomingStartRequest;
    bool confirmAcceptSelected;
};

struct GameplayRenderState {
    PlayerRenderState player;
    bool remoteFinished;
};

struct PauseRenderState {
    PlayerRenderState player;
    PausePanelState panelState;
    PauseMenuAction selectedAction;
    PauseMenuAction pendingAction;
    bool confirmAcceptSelected;
};

struct GameOverRenderState {
    PlayerRenderState player;
    GameOverReason reason;
    bool remoteFinished;
    bool localWon;
    bool resultReady;
    bool disconnected;
    GameOverPanelState panelState;
    PostGameMenuAction selectedAction;
    PostGameMenuAction pendingAction;
    bool confirmAcceptSelected;
};

struct ScreenRenderState {
    RenderScreen screen;
    union {
        LobbyRenderState lobby;
        GameplayRenderState gameplay;
        PauseRenderState pause;
        GameOverRenderState gameOver;
    } payload;
};

struct RenderEvent {
    RenderEventType type;
    union {
        DisplayRenderConfig config;
        ScreenRenderState screen;
    } payload;
};


// GameTask <-> StorageTask

enum class StorageKey : uint8_t {
    UserSettings,
    MatchSettings
};

enum class StorageOperation : uint8_t {
    Load,
    Save
};

struct StorageRequest {
    StorageOperation operation;
    StorageKey key;
    char localUsername[MAX_USERNAME_LEN + 1];
    char remoteUsername[MAX_USERNAME_LEN + 1];
    union {
        UserSettings userSettings;
        MatchSettings matchSettings;
    } payload;
};

enum class StorageStatus : uint8_t {
    Success,
    NotFound,
    Failed
};

struct StorageResponse {
    StorageStatus status;
    StorageKey key;
    union {
        UserSettings userSettings;
        MatchSettings matchSettings;
    } payload;
};
