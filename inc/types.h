#pragma once

#include <Arduino.h>

static constexpr uint8_t MAX_USERNAME_LEN     = 8;
static constexpr uint8_t ESPNOW_PEER_ADDR_LEN = 6;
static constexpr uint8_t BOARD_WIDTH          = 10;
static constexpr uint8_t BOARD_HEIGHT         = 20;
static constexpr uint8_t MAX_NEXT_PIECES      = 5;

enum class PhysicalButton : uint8_t {
    LdUp,
    LdLeft,
    LdRight,
    LdDown,
    RdUp,
    RdLeft,
    RdRight,
    RdDown,
    Center
};

enum class ThemeId : uint8_t {
    Gameboy,
    Nintendo,
    Modern
};

enum class GameMode : uint8_t {
    Marathon,
    Sprint40
};

enum class PauseMenuAction : uint8_t {
    Resume,
    Restart,
    Quit
};

enum class GameOverReason : uint8_t {
    TopOut,
    GarbageCrush,
    SprintComplete,
    Quit,
    Disconnect
};

enum class TetrominoType : uint8_t {
    None,
    I,
    O,
    T,
    S,
    Z,
    J,
    L,
    Garbage
};

struct ButtonMapping {
    PhysicalButton moveLeft;
    PhysicalButton moveRight;
    PhysicalButton softDrop;
    PhysicalButton hardDrop;
    PhysicalButton rotateCw;
    PhysicalButton rotateCcw;
    PhysicalButton hold;
};

struct UserSettings {
    char username[MAX_USERNAME_LEN + 1];
    ButtonMapping buttonMapping;
    ThemeId theme;
    bool holdEnabled;
    bool ghostEnabled;
    uint8_t nextPreviewCount;
};

// Match settings are shared between the two players for a given game session.
struct MatchSettings {
    bool garbageEnabled;
    GameMode mode;
    uint8_t startingLevel;
};

struct BoardState {
    uint8_t cells[BOARD_HEIGHT][BOARD_WIDTH];
};

struct ActivePieceState {
    TetrominoType type;
    int8_t x;
    int8_t y;
    uint8_t rotation;
};

struct PlayerRenderState {
    BoardState board;
    ActivePieceState activePiece;
    bool hasGhostPiece;
    ActivePieceState ghostPiece;
    bool hasHoldPiece;
    TetrominoType holdPiece;
    uint8_t nextPieceCount;
    TetrominoType nextPieces[MAX_NEXT_PIECES];
    uint32_t score;
    uint16_t linesCleared;
    uint8_t level;
};
