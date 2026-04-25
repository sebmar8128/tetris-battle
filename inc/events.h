#pragma once

#include <Arduino.h>

// InputTask -> GameTask

enum class InputAction : uint8_t {
    MoveLeft,
    MoveRight,
    SoftDrop,
    HardDrop,
    RotateCW,
    RotateCCW,
    Hold,
    Pause
};

enum class InputEventType : uint8_t {
    Press,
    Release,
    Repeat
};

struct InputEvent {
    InputAction action;
    InputEventType type;
};


// WirelessTask -> GameTask

enum class RemoteEvent : uint8_t {
    PacketReceived,
    SendComplete,
    PeerTimeout
};


// GameTask -> WirelessTask

enum class OutboundMessage : uint8_t {
    GameStart,
    Input,
    Pause,
    ResumeRequest,
    ResumeConfirm,
    RestartRequest,
    RestartConfirm,
    QuitRequest,
    QuitConfirm,
    Garbage
};


// GameTask -> RenderTask

enum class RenderCommand : uint8_t {
    FullRedraw,
    DrawMenu,
    DrawBoard,
    DrawPauseScreen,
    DrawGameOver
};


// GameTask -> StorageTask

enum class StorageRequest : uint8_t {
    LoadUserSettings,
    SaveUserSettings,
    LoadPairSettings,
    SavePairSettings
};


// StorageTask -> GameTask

enum class StorageResponse : uint8_t {
    LoadSuccess,
    LoadNotFound,
    SaveSuccess,
    SaveFailed
};