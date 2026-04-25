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

enum class PacketType : uint8_t {
    LobbyState,
    StartGameRequest,
    StartGameReply,
    Pause,
    PauseActionRequest,
    PauseActionReply,
    Garbage,
    GameOver
};

struct LobbyStatePacket {
    char username[MAX_USERNAME_LEN + 1];
    bool ready;
};

struct StartGameRequestPacket {
    uint8_t requestId;
    uint32_t seed;
    MatchSettings settings;
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
};

struct PauseActionReplyPacket {
    uint8_t pauseId;
    uint8_t requestId;
    PauseMenuAction action;
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
    PacketType type;
    union {
        LobbyStatePacket lobbyState;
        StartGameRequestPacket startGameRequest;
        StartGameReplyPacket startGameReply;
        PausePacket pause;
        PauseActionRequestPacket pauseActionRequest;
        PauseActionReplyPacket pauseActionReply;
        GarbagePacket garbage;
        GameOverPacket gameOver;
    } payload;
};

enum class RemoteEventType : uint8_t {
    PacketReceived,
    PeerConnected,
    PeerDisconnected
};

struct RemoteEvent {
    RemoteEventType type;
    uint8_t peerAddress[ESPNOW_PEER_ADDR_LEN];
    NetPacket packet;
};


// GameTask -> RenderTask

enum class RenderScreen : uint8_t {
    Lobby,
    Gameplay,
    Paused,
    GameOver
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
    bool localReady;
    bool remoteReady;
};

struct GameplayRenderState {
    PlayerRenderState player;
};

struct PauseRenderState {
    PlayerRenderState player;
    bool isPauseOwner;
    bool showPauseMenu;
};

struct GameOverRenderState {
    PlayerRenderState player;
    GameOverReason reason;
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
