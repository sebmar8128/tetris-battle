#pragma once

#include <Arduino.h>

#include "types.h"

namespace Tetromino {

static constexpr uint8_t SHAPE_SIZE = 4;
static constexpr uint8_t ROTATION_COUNT = 4;
static constexpr uint8_t WALL_KICK_TEST_COUNT = 5;

enum class RotationState : uint8_t {
    Spawn = 0,
    Right = 1,
    Reverse = 2,
    Left = 3
};

struct KickOffset {
    int8_t x;
    int8_t y;
};

bool hasCell(TetrominoType type, uint8_t rotation, int8_t x, int8_t y);
const KickOffset* wallKicks(TetrominoType type, RotationState from, RotationState to);

}  // namespace Tetromino
