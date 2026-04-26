#pragma once

#include <Arduino.h>

#include "types.h"

namespace GameEngine {

static constexpr uint8_t HIDDEN_ROWS = 2;
static constexpr uint8_t VISIBLE_ROWS = BOARD_HEIGHT;
static constexpr uint8_t TOTAL_ROWS = HIDDEN_ROWS + VISIBLE_ROWS;

enum class Action : uint8_t {
    MoveLeft,
    MoveRight,
    SoftDropPressed,
    SoftDropReleased,
    HardDrop,
    RotateCw,
    RotateCcw,
    Hold
};

struct Config {
    uint32_t seed;
    MatchSettings matchSettings;
    UserSettings userSettings;
};

class Engine {
public:
    void begin(const Config& config);
    bool tick(uint32_t nowMs);
    bool applyAction(Action action, uint32_t nowMs);

    bool isGameOver() const;
    GameOverReason gameOverReason() const;
    PlayerRenderState renderState() const;

private:
    void clear();
    void refillNextQueue();
    void refillBag();
    TetrominoType drawPiece();
    TetrominoType popNextPiece();

    bool canPlace(TetrominoType type, int8_t x, int8_t y, uint8_t rotation) const;
    void spawnNextPiece();
    void spawnPiece(TetrominoType type);
    bool tryMove(int8_t dx, int8_t dy);
    bool tryRotate(int8_t direction);
    void hardDrop();
    void hold();
    void lockActivePiece();
    uint8_t clearLines();
    void addLineClearScore(uint8_t clearedLines);
    void updateLevel();
    ActivePieceState ghostPiece() const;
    uint32_t gravityIntervalMs() const;
    uint32_t nextRandom();
    uint8_t randomIndex(uint8_t exclusiveMax);

    uint8_t board_[TOTAL_ROWS][BOARD_WIDTH];
    ActivePieceState activePiece_;
    TetrominoType holdPiece_;
    bool hasHoldPiece_;
    bool holdUsedThisTurn_;
    bool softDropHeld_;
    bool gameOver_;
    GameOverReason gameOverReason_;

    TetrominoType bag_[7];
    uint8_t bagIndex_;
    TetrominoType nextQueue_[MAX_NEXT_PIECES];
    uint8_t nextQueueCount_;
    uint32_t rngState_;

    MatchSettings matchSettings_;
    UserSettings userSettings_;
    uint32_t score_;
    uint16_t linesCleared_;
    uint8_t level_;
    uint16_t firstLevelUpLines_;
    uint32_t nextGravityMs_;
};

}  // namespace GameEngine
