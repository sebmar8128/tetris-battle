#include "game_engine.h"

#include <string.h>

#include "tetromino.h"

namespace {

constexpr uint8_t BAG_SIZE = 7;
constexpr int8_t SPAWN_X = 3;
constexpr int8_t SPAWN_Y = 0;
constexpr uint32_t SOFT_DROP_INTERVAL_MS = 50;

constexpr uint8_t NES_GRAVITY_FRAMES[] = {
    48, 43, 38, 33, 28, 23, 18, 13, 8, 6,
    5, 5, 5,
    4, 4, 4,
    3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2
};

constexpr TetrominoType BAG_PIECES[BAG_SIZE] = {
    TetrominoType::I,
    TetrominoType::O,
    TetrominoType::T,
    TetrominoType::S,
    TetrominoType::Z,
    TetrominoType::J,
    TetrominoType::L
};

uint8_t gravityFramesForLevel(uint8_t level) {
    if (level < sizeof(NES_GRAVITY_FRAMES)) {
        return NES_GRAVITY_FRAMES[level];
    }

    return 1;
}

uint16_t firstLevelUpThreshold(uint8_t startingLevel) {
    const uint16_t simpleThreshold = static_cast<uint16_t>(startingLevel) * 10U + 10U;
    const int16_t highLevelThreshold = static_cast<int16_t>(startingLevel) * 10 - 50;
    const uint16_t cappedThreshold = highLevelThreshold > 100
        ? static_cast<uint16_t>(highLevelThreshold)
        : 100U;

    return simpleThreshold < cappedThreshold ? simpleThreshold : cappedThreshold;
}

uint32_t scoreForLines(uint8_t clearedLines, uint8_t level) {
    const uint32_t multiplier = static_cast<uint32_t>(level) + 1U;

    switch (clearedLines) {
        case 1: return 40U * multiplier;
        case 2: return 100U * multiplier;
        case 3: return 300U * multiplier;
        case 4: return 1200U * multiplier;
        default:
            break;
    }

    return 0;
}

uint8_t garbageForLines(uint8_t clearedLines) {
    switch (clearedLines) {
        case 2: return 1;
        case 3: return 2;
        case 4: return 4;
        default:
            break;
    }

    return 0;
}

Tetromino::RotationState rotationState(uint8_t rotation) {
    return static_cast<Tetromino::RotationState>(rotation % Tetromino::ROTATION_COUNT);
}

uint8_t rotated(uint8_t rotation, int8_t direction) {
    return static_cast<uint8_t>((rotation + Tetromino::ROTATION_COUNT + direction) %
                                Tetromino::ROTATION_COUNT);
}

}  // namespace

namespace GameEngine {

void Engine::begin(const Config& config) {
    clear();

    matchSettings_ = config.matchSettings;
    userSettings_ = config.userSettings;
    userSettings_.nextPreviewCount = min(userSettings_.nextPreviewCount, MAX_NEXT_PIECES);

    rngState_ = config.seed != 0 ? config.seed : 1U;
    level_ = matchSettings_.startingLevel;
    firstLevelUpLines_ = firstLevelUpThreshold(matchSettings_.startingLevel);

    refillBag();
    refillNextQueue();
    spawnNextPiece();
}

StepResult Engine::tick(uint32_t nowMs) {
    StepResult result = {};

    if (gameOver_) {
        result.gameOver = true;
        return result;
    }

    if (nextGravityMs_ == 0) {
        nextGravityMs_ = nowMs + gravityIntervalMs();
        return result;
    }

    if (static_cast<int32_t>(nowMs - nextGravityMs_) < 0) {
        return result;
    }

    if (!tryMove(0, 1)) {
        result = lockActivePiece();
    } else {
        result.changed = true;
    }

    nextGravityMs_ = nowMs + gravityIntervalMs();
    result.gameOver = gameOver_;
    return result;
}

StepResult Engine::applyAction(Action action, uint32_t nowMs) {
    StepResult result = {};

    if (gameOver_) {
        result.gameOver = true;
        return result;
    }

    switch (action) {
        case Action::MoveLeft:
            result.changed = tryMove(-1, 0);
            break;
        case Action::MoveRight:
            result.changed = tryMove(1, 0);
            break;
        case Action::SoftDropPressed:
            if (!softDropHeld_) {
                softDropHeld_ = true;
                result.changed = true;
                if (nextGravityMs_ == 0 ||
                    static_cast<int32_t>(nextGravityMs_ - (nowMs + gravityIntervalMs())) > 0) {
                    nextGravityMs_ = nowMs + gravityIntervalMs();
                }
            }
            break;
        case Action::SoftDropReleased:
            if (softDropHeld_) {
                softDropHeld_ = false;
                nextGravityMs_ = nowMs + gravityIntervalMs();
                result.changed = true;
            }
            break;
        case Action::HardDrop:
            result = hardDrop();
            break;
        case Action::RotateCw:
            result.changed = tryRotate(1);
            break;
        case Action::RotateCcw:
            result.changed = tryRotate(-1);
            break;
        case Action::Hold:
        {
            const TetrominoType previousActive = activePiece_.type;
            const TetrominoType previousHold = holdPiece_;
            const bool previousHasHold = hasHoldPiece_;
            hold();
            result.changed = previousActive != activePiece_.type ||
                             previousHold != holdPiece_ ||
                             previousHasHold != hasHoldPiece_ ||
                             gameOver_;
            break;
        }
    }

    if (nextGravityMs_ == 0) {
        nextGravityMs_ = nowMs + gravityIntervalMs();
    }

    result.gameOver = gameOver_;
    return result;
}

StepResult Engine::applyGarbage(const GarbageAttack& garbage) {
    StepResult result = {};

    if (gameOver_) {
        result.gameOver = true;
        return result;
    }

    const uint8_t lines = min(garbage.lines, TOTAL_ROWS);
    if (lines == 0) {
        return result;
    }

    const uint8_t holeColumn = garbage.holeColumn < BOARD_WIDTH
        ? garbage.holeColumn
        : static_cast<uint8_t>(garbage.holeColumn % BOARD_WIDTH);

    bool crushed = false;
    for (uint8_t row = 0; row < lines; ++row) {
        for (uint8_t col = 0; col < BOARD_WIDTH; ++col) {
            if (board_[row][col] != static_cast<uint8_t>(TetrominoType::None)) {
                crushed = true;
                break;
            }
        }
    }

    for (uint8_t row = 0; row < TOTAL_ROWS - lines; ++row) {
        memcpy(board_[row], board_[row + lines], sizeof(board_[row]));
    }

    for (uint8_t row = TOTAL_ROWS - lines; row < TOTAL_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_WIDTH; ++col) {
            board_[row][col] = col == holeColumn
                ? static_cast<uint8_t>(TetrominoType::None)
                : static_cast<uint8_t>(TetrominoType::Garbage);
        }
    }

    if (!canPlace(activePiece_.type, activePiece_.x, activePiece_.y, activePiece_.rotation)) {
        crushed = true;
    }

    if (crushed) {
        gameOver_ = true;
        gameOverReason_ = GameOverReason::GarbageCrush;
    }

    result.changed = true;
    result.gameOver = gameOver_;
    return result;
}

bool Engine::isGameOver() const {
    return gameOver_;
}

GameOverReason Engine::gameOverReason() const {
    return gameOverReason_;
}

PlayerRenderState Engine::renderState() const {
    PlayerRenderState state = {};

    for (uint8_t row = 0; row < VISIBLE_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_WIDTH; ++col) {
            state.board.cells[row][col] = board_[row + HIDDEN_ROWS][col];
        }
    }

    state.activePiece = activePiece_;
    state.activePiece.y -= HIDDEN_ROWS;
    state.hasGhostPiece = userSettings_.ghostEnabled && !gameOver_;
    state.ghostPiece = ghostPiece();
    state.ghostPiece.y -= HIDDEN_ROWS;
    state.hasHoldPiece = userSettings_.holdEnabled && hasHoldPiece_;
    state.holdPiece = state.hasHoldPiece ? holdPiece_ : TetrominoType::None;
    state.nextPieceCount = min(nextQueueCount_, userSettings_.nextPreviewCount);

    for (uint8_t i = 0; i < state.nextPieceCount; ++i) {
        state.nextPieces[i] = nextQueue_[i];
    }

    state.score = score_;
    state.linesCleared = linesCleared_;
    state.level = level_;

    return state;
}

void Engine::clear() {
    memset(board_, 0, sizeof(board_));
    activePiece_ = {TetrominoType::None, SPAWN_X, SPAWN_Y, 0};
    holdPiece_ = TetrominoType::None;
    hasHoldPiece_ = false;
    holdUsedThisTurn_ = false;
    softDropHeld_ = false;
    gameOver_ = false;
    gameOverReason_ = GameOverReason::TopOut;
    bagIndex_ = BAG_SIZE;
    nextQueueCount_ = 0;
    rngState_ = 1;
    score_ = 0;
    linesCleared_ = 0;
    level_ = 0;
    firstLevelUpLines_ = 10;
    nextGravityMs_ = 0;

    for (uint8_t i = 0; i < MAX_NEXT_PIECES; ++i) {
        nextQueue_[i] = TetrominoType::None;
    }
}

void Engine::refillNextQueue() {
    while (nextQueueCount_ < MAX_NEXT_PIECES) {
        nextQueue_[nextQueueCount_] = drawPiece();
        ++nextQueueCount_;
    }
}

void Engine::refillBag() {
    for (uint8_t i = 0; i < BAG_SIZE; ++i) {
        bag_[i] = BAG_PIECES[i];
    }

    for (uint8_t i = BAG_SIZE - 1; i > 0; --i) {
        const uint8_t j = randomIndex(i + 1);
        const TetrominoType temp = bag_[i];
        bag_[i] = bag_[j];
        bag_[j] = temp;
    }

    bagIndex_ = 0;
}

TetrominoType Engine::drawPiece() {
    if (bagIndex_ >= BAG_SIZE) {
        refillBag();
    }

    return bag_[bagIndex_++];
}

TetrominoType Engine::popNextPiece() {
    refillNextQueue();

    const TetrominoType piece = nextQueue_[0];
    for (uint8_t i = 1; i < nextQueueCount_; ++i) {
        nextQueue_[i - 1] = nextQueue_[i];
    }
    --nextQueueCount_;

    refillNextQueue();
    return piece;
}

bool Engine::canPlace(TetrominoType type, int8_t x, int8_t y, uint8_t rotation) const {
    if (type == TetrominoType::None) {
        return false;
    }

    for (int8_t py = 0; py < Tetromino::SHAPE_SIZE; ++py) {
        for (int8_t px = 0; px < Tetromino::SHAPE_SIZE; ++px) {
            if (!Tetromino::hasCell(type, rotation, px, py)) {
                continue;
            }

            const int8_t boardX = x + px;
            const int8_t boardY = y + py;

            if (boardX < 0 || boardX >= BOARD_WIDTH || boardY < 0 || boardY >= TOTAL_ROWS) {
                return false;
            }

            if (board_[boardY][boardX] != static_cast<uint8_t>(TetrominoType::None)) {
                return false;
            }
        }
    }

    return true;
}

void Engine::spawnNextPiece() {
    spawnPiece(popNextPiece());
}

void Engine::spawnPiece(TetrominoType type) {
    activePiece_ = {type, SPAWN_X, SPAWN_Y, 0};
    holdUsedThisTurn_ = false;
    nextGravityMs_ = 0;

    if (!canPlace(activePiece_.type, activePiece_.x, activePiece_.y, activePiece_.rotation)) {
        gameOver_ = true;
        gameOverReason_ = GameOverReason::TopOut;
    }
}

bool Engine::tryMove(int8_t dx, int8_t dy) {
    const int8_t nextX = activePiece_.x + dx;
    const int8_t nextY = activePiece_.y + dy;

    if (!canPlace(activePiece_.type, nextX, nextY, activePiece_.rotation)) {
        return false;
    }

    activePiece_.x = nextX;
    activePiece_.y = nextY;
    return true;
}

bool Engine::tryRotate(int8_t direction) {
    const uint8_t nextRotation = rotated(activePiece_.rotation, direction);
    const Tetromino::RotationState from = rotationState(activePiece_.rotation);
    const Tetromino::RotationState to = rotationState(nextRotation);
    const Tetromino::KickOffset* kicks = Tetromino::wallKicks(activePiece_.type, from, to);

    for (uint8_t i = 0; i < Tetromino::WALL_KICK_TEST_COUNT; ++i) {
        const int8_t nextX = activePiece_.x + kicks[i].x;
        const int8_t nextY = activePiece_.y + kicks[i].y;

        if (canPlace(activePiece_.type, nextX, nextY, nextRotation)) {
            activePiece_.x = nextX;
            activePiece_.y = nextY;
            activePiece_.rotation = nextRotation;
            return true;
        }
    }

    return false;
}

StepResult Engine::hardDrop() {
    activePiece_ = ghostPiece();
    StepResult result = lockActivePiece();
    result.changed = true;
    return result;
}

void Engine::hold() {
    if (!userSettings_.holdEnabled || holdUsedThisTurn_) {
        return;
    }

    const TetrominoType previousActive = activePiece_.type;
    holdUsedThisTurn_ = true;

    if (hasHoldPiece_) {
        spawnPiece(holdPiece_);
    } else {
        hasHoldPiece_ = true;
        spawnNextPiece();
    }

    holdPiece_ = previousActive;
    holdUsedThisTurn_ = true;
}

StepResult Engine::lockActivePiece() {
    StepResult result = {};

    for (int8_t py = 0; py < Tetromino::SHAPE_SIZE; ++py) {
        for (int8_t px = 0; px < Tetromino::SHAPE_SIZE; ++px) {
            if (!Tetromino::hasCell(activePiece_.type, activePiece_.rotation, px, py)) {
                continue;
            }

            const int8_t boardX = activePiece_.x + px;
            const int8_t boardY = activePiece_.y + py;
            if (boardX >= 0 && boardX < BOARD_WIDTH && boardY >= 0 && boardY < TOTAL_ROWS) {
                board_[boardY][boardX] = static_cast<uint8_t>(activePiece_.type);
            }
        }
    }

    const uint8_t clearedLines = clearLines();
    result.clearedLines = clearedLines;
    addLineClearScore(clearedLines);
    linesCleared_ += clearedLines;
    updateLevel();
    addOutgoingGarbage(result, clearedLines);

    if (matchSettings_.mode == GameMode::Sprint40 && linesCleared_ >= 40) {
        gameOver_ = true;
        gameOverReason_ = GameOverReason::SprintComplete;
        result.changed = true;
        result.gameOver = true;
        return result;
    }

    spawnNextPiece();
    result.changed = true;
    result.gameOver = gameOver_;
    return result;
}

uint8_t Engine::clearLines() {
    uint8_t clearedLines = 0;

    for (int8_t row = TOTAL_ROWS - 1; row >= 0; --row) {
        bool full = true;
        for (uint8_t col = 0; col < BOARD_WIDTH; ++col) {
            if (board_[row][col] == static_cast<uint8_t>(TetrominoType::None)) {
                full = false;
                break;
            }
        }

        if (!full) {
            continue;
        }

        ++clearedLines;
        for (int8_t moveRow = row; moveRow > 0; --moveRow) {
            memcpy(board_[moveRow], board_[moveRow - 1], sizeof(board_[moveRow]));
        }
        memset(board_[0], 0, sizeof(board_[0]));
        ++row;
    }

    return clearedLines;
}

void Engine::addLineClearScore(uint8_t clearedLines) {
    score_ += scoreForLines(clearedLines, level_);
}

void Engine::addOutgoingGarbage(StepResult& result, uint8_t clearedLines) {
    if (!matchSettings_.garbageEnabled) {
        return;
    }

    const uint8_t lines = garbageForLines(clearedLines);
    if (lines == 0) {
        return;
    }

    result.hasOutgoingGarbage = true;
    result.outgoingGarbage = {
        lines,
        randomIndex(BOARD_WIDTH)
    };
}

void Engine::updateLevel() {
    if (linesCleared_ < firstLevelUpLines_) {
        return;
    }

    const uint16_t linesAfterFirstLevel = linesCleared_ - firstLevelUpLines_;
    level_ = matchSettings_.startingLevel + 1 + (linesAfterFirstLevel / 10);
}

ActivePieceState Engine::ghostPiece() const {
    ActivePieceState ghost = activePiece_;

    while (canPlace(ghost.type, ghost.x, ghost.y + 1, ghost.rotation)) {
        ++ghost.y;
    }

    return ghost;
}

uint32_t Engine::gravityIntervalMs() const {
    const uint32_t normalIntervalMs = (static_cast<uint32_t>(gravityFramesForLevel(level_)) * 1000U) / 60U;

    if (softDropHeld_ && SOFT_DROP_INTERVAL_MS < normalIntervalMs) {
        return SOFT_DROP_INTERVAL_MS;
    }

    return normalIntervalMs > 0 ? normalIntervalMs : 1U;
}

uint32_t Engine::nextRandom() {
    rngState_ = rngState_ * 1664525UL + 1013904223UL;
    return rngState_;
}

uint8_t Engine::randomIndex(uint8_t exclusiveMax) {
    return static_cast<uint8_t>(nextRandom() % exclusiveMax);
}

}  // namespace GameEngine
