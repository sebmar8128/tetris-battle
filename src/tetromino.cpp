#include "tetromino.h"

namespace {

using ShapeMask = uint16_t;

constexpr uint8_t INVALID_PIECE_INDEX = 0xFF;

uint8_t pieceIndex(TetrominoType type) {
    switch (type) {
        case TetrominoType::I: return 0;
        case TetrominoType::O: return 1;
        case TetrominoType::T: return 2;
        case TetrominoType::S: return 3;
        case TetrominoType::Z: return 4;
        case TetrominoType::J: return 5;
        case TetrominoType::L: return 6;
        case TetrominoType::Garbage:
        case TetrominoType::None:
            break;
    }

    return INVALID_PIECE_INDEX;
}

constexpr ShapeMask cell(uint8_t x, uint8_t y) {
    return static_cast<ShapeMask>(1U << (y * Tetromino::SHAPE_SIZE + x));
}

constexpr ShapeMask shape(
    uint8_t x0,
    uint8_t y0,
    uint8_t x1,
    uint8_t y1,
    uint8_t x2,
    uint8_t y2,
    uint8_t x3,
    uint8_t y3
) {
    return cell(x0, y0) | cell(x1, y1) | cell(x2, y2) | cell(x3, y3);
}

constexpr Tetromino::KickOffset kick(int8_t x, int8_t y) {
    return {x, y};
}

/*
SRS rotation states use the order 0, R, 2, L.
Each shape is a 4x4 mask where bit = y * 4 + x:

  bit 0  bit 1  bit 2  bit 3
  bit 4  bit 5  bit 6  bit 7
  bit 8  bit 9  bit 10 bit 11
  bit 12 bit 13 bit 14 bit 15

The row diagrams below are intentionally kept beside the masks because these
tables are shared by rendering now and by collision/rotation logic later.
*/
constexpr ShapeMask SRS_SHAPES[7][Tetromino::ROTATION_COUNT] = {
    // I
    {
        shape(0, 1, 1, 1, 2, 1, 3, 1), // 0: .... / XXXX / .... / ....
        shape(2, 0, 2, 1, 2, 2, 2, 3), // R: ..X. / ..X. / ..X. / ..X.
        shape(0, 2, 1, 2, 2, 2, 3, 2), // 2: .... / .... / XXXX / ....
        shape(1, 0, 1, 1, 1, 2, 1, 3), // L: .X.. / .X.. / .X.. / .X..
    },

    // O
    {
        shape(1, 1, 2, 1, 1, 2, 2, 2), // 0: .... / .XX. / .XX. / ....
        shape(1, 1, 2, 1, 1, 2, 2, 2), // R: .... / .XX. / .XX. / ....
        shape(1, 1, 2, 1, 1, 2, 2, 2), // 2: .... / .XX. / .XX. / ....
        shape(1, 1, 2, 1, 1, 2, 2, 2), // L: .... / .XX. / .XX. / ....
    },

    // T
    {
        shape(0, 1, 1, 1, 2, 1, 1, 2), // 0: .... / XXX. / .X.. / ....
        shape(1, 0, 0, 1, 1, 1, 1, 2), // R: .X.. / XX.. / .X.. / ....
        shape(1, 0, 0, 1, 1, 1, 2, 1), // 2: .X.. / XXX. / .... / ....
        shape(1, 0, 1, 1, 2, 1, 1, 2), // L: .X.. / .XX. / .X.. / ....
    },

    // S
    {
        shape(1, 1, 2, 1, 0, 2, 1, 2), // 0: .... / .XX. / XX.. / ....
        shape(1, 0, 1, 1, 2, 1, 2, 2), // R: .X.. / .XX. / ..X. / ....
        shape(1, 1, 2, 1, 0, 2, 1, 2), // 2: .... / .XX. / XX.. / ....
        shape(1, 0, 1, 1, 2, 1, 2, 2), // L: .X.. / .XX. / ..X. / ....
    },

    // Z
    {
        shape(0, 1, 1, 1, 1, 2, 2, 2), // 0: .... / XX.. / .XX. / ....
        shape(2, 0, 1, 1, 2, 1, 1, 2), // R: ..X. / .XX. / .X.. / ....
        shape(0, 1, 1, 1, 1, 2, 2, 2), // 2: .... / XX.. / .XX. / ....
        shape(2, 0, 1, 1, 2, 1, 1, 2), // L: ..X. / .XX. / .X.. / ....
    },

    // J
    {
        shape(0, 0, 0, 1, 1, 1, 2, 1), // 0: X... / XXX. / .... / ....
        shape(1, 0, 2, 0, 1, 1, 1, 2), // R: .XX. / .X.. / .X.. / ....
        shape(0, 1, 1, 1, 2, 1, 2, 2), // 2: .... / XXX. / ..X. / ....
        shape(1, 0, 1, 1, 0, 2, 1, 2), // L: .X.. / .X.. / XX.. / ....
    },

    // L
    {
        shape(2, 0, 0, 1, 1, 1, 2, 1), // 0: ..X. / XXX. / .... / ....
        shape(1, 0, 1, 1, 1, 2, 2, 2), // R: .X.. / .X.. / .XX. / ....
        shape(0, 1, 1, 1, 2, 1, 0, 2), // 2: .... / XXX. / X... / ....
        shape(0, 0, 1, 0, 1, 1, 1, 2), // L: XX.. / .X.. / .X.. / ....
    },
};

constexpr Tetromino::KickOffset NO_KICKS[Tetromino::WALL_KICK_TEST_COUNT] = {
    kick(0, 0), kick(0, 0), kick(0, 0), kick(0, 0), kick(0, 0)
};

/* 
TetrisWiki documents SRS kicks with positive y upward. This project uses
screen-style board coordinates where positive y is downward, so the y values
below are inverted from the published SRS tables.
*/
constexpr Tetromino::KickOffset JLSTZ_KICKS[8][Tetromino::WALL_KICK_TEST_COUNT] = {
    {kick( 0,  0), kick(-1,  0), kick(-1, -1), kick( 0, +2), kick(-1, +2)}, // 0 -> R
    {kick( 0,  0), kick(+1,  0), kick(+1, +1), kick( 0, -2), kick(+1, -2)}, // R -> 0
    {kick( 0,  0), kick(+1,  0), kick(+1, +1), kick( 0, -2), kick(+1, -2)}, // R -> 2
    {kick( 0,  0), kick(-1,  0), kick(-1, -1), kick( 0, +2), kick(-1, +2)}, // 2 -> R
    {kick( 0,  0), kick(+1,  0), kick(+1, -1), kick( 0, +2), kick(+1, +2)}, // 2 -> L
    {kick( 0,  0), kick(-1,  0), kick(-1, +1), kick( 0, -2), kick(-1, -2)}, // L -> 2
    {kick( 0,  0), kick(-1,  0), kick(-1, +1), kick( 0, -2), kick(-1, -2)}, // L -> 0
    {kick( 0,  0), kick(+1,  0), kick(+1, -1), kick( 0, +2), kick(+1, +2)}, // 0 -> L
};

constexpr Tetromino::KickOffset I_KICKS[8][Tetromino::WALL_KICK_TEST_COUNT] = {
    {kick( 0,  0), kick(-2,  0), kick(+1,  0), kick(-2, +1), kick(+1, -2)}, // 0 -> R
    {kick( 0,  0), kick(+2,  0), kick(-1,  0), kick(+2, -1), kick(-1, +2)}, // R -> 0
    {kick( 0,  0), kick(-1,  0), kick(+2,  0), kick(-1, -2), kick(+2, +1)}, // R -> 2
    {kick( 0,  0), kick(+1,  0), kick(-2,  0), kick(+1, +2), kick(-2, -1)}, // 2 -> R
    {kick( 0,  0), kick(+2,  0), kick(-1,  0), kick(+2, -1), kick(-1, +2)}, // 2 -> L
    {kick( 0,  0), kick(-2,  0), kick(+1,  0), kick(-2, +1), kick(+1, -2)}, // L -> 2
    {kick( 0,  0), kick(+1,  0), kick(-2,  0), kick(+1, +2), kick(-2, -1)}, // L -> 0
    {kick( 0,  0), kick(-1,  0), kick(+2,  0), kick(-1, -2), kick(+2, +1)}, // 0 -> L
};

uint8_t kickIndex(Tetromino::RotationState from, Tetromino::RotationState to) {
    using Tetromino::RotationState;

    if (from == RotationState::Spawn && to == RotationState::Right) return 0;
    if (from == RotationState::Right && to == RotationState::Spawn) return 1;
    if (from == RotationState::Right && to == RotationState::Reverse) return 2;
    if (from == RotationState::Reverse && to == RotationState::Right) return 3;
    if (from == RotationState::Reverse && to == RotationState::Left) return 4;
    if (from == RotationState::Left && to == RotationState::Reverse) return 5;
    if (from == RotationState::Left && to == RotationState::Spawn) return 6;
    if (from == RotationState::Spawn && to == RotationState::Left) return 7;

    return 0xFF;
}

}  // namespace

namespace Tetromino {

bool hasCell(TetrominoType type, uint8_t rotation, int8_t x, int8_t y) {
    if (x < 0 || x >= SHAPE_SIZE || y < 0 || y >= SHAPE_SIZE) {
        return false;
    }

    const uint8_t index = pieceIndex(type);
    if (index == INVALID_PIECE_INDEX) {
        return false;
    }

    const ShapeMask mask = SRS_SHAPES[index][rotation % ROTATION_COUNT];
    const uint8_t bit = static_cast<uint8_t>(y) * SHAPE_SIZE + static_cast<uint8_t>(x);
    return (mask & static_cast<ShapeMask>(1U << bit)) != 0;
}

const KickOffset* wallKicks(TetrominoType type, RotationState from, RotationState to) {
    if (type == TetrominoType::None || type == TetrominoType::Garbage || from == to) {
        return NO_KICKS;
    }

    const uint8_t index = kickIndex(from, to);
    if (index >= 8 || type == TetrominoType::O) {
        return NO_KICKS;
    }

    if (type == TetrominoType::I) {
        return I_KICKS[index];
    }

    return JLSTZ_KICKS[index];
}

}  // namespace Tetromino
