#include "display.h"

#include <TFT_eSPI.h>

#include "tetromino.h"

namespace {

struct ThemePalette {
    uint16_t background;
    uint16_t panel;
    uint16_t grid;
    uint16_t text;
    uint16_t mutedText;
    uint16_t accent;
    uint16_t pieceI;
    uint16_t pieceO;
    uint16_t pieceT;
    uint16_t pieceS;
    uint16_t pieceZ;
    uint16_t pieceJ;
    uint16_t pieceL;
    uint16_t ghost;
};

struct BoardLayout {
    int16_t boardX;
    int16_t boardY;
    int16_t cellSize;
};

struct RenderCell {
    TetrominoType type;
    bool ghost;
};

struct GameplayRenderCache {
    bool valid;
    PlayerRenderState player;
};

TFT_eSPI tft;
bool initialized = false;
bool hasCurrentScreen = false;
RenderScreen currentScreen = RenderScreen::Gameplay;
GameplayRenderCache gameplayCache = {};

DisplayRenderConfig renderConfig = {
    ThemeId::Modern,
    true,
    true,
    MAX_NEXT_PIECES
};

constexpr BoardLayout GAMEPLAY_LAYOUT = {150, 34, 12};
constexpr int16_t PREVIEW_CELL_SIZE = 10;
constexpr int16_t HOLD_PANEL_X = 18;
constexpr int16_t HOLD_PANEL_Y = 34;
constexpr int16_t HOLD_PANEL_W = 104;
constexpr int16_t HOLD_PANEL_H = 74;
constexpr int16_t HOLD_PREVIEW_X = 48;
constexpr int16_t HOLD_PREVIEW_Y = 60;
constexpr int16_t NEXT_PANEL_X = 356;
constexpr int16_t NEXT_PANEL_Y = 34;
constexpr int16_t NEXT_PANEL_W = 104;
constexpr int16_t NEXT_PANEL_H = 210;
constexpr int16_t NEXT_PREVIEW_X = 386;
constexpr int16_t NEXT_PREVIEW_Y = 62;
constexpr int16_t STATS_PANEL_X = 18;
constexpr int16_t STATS_PANEL_Y = 130;
constexpr int16_t STATS_PANEL_W = 104;
constexpr int16_t STATS_PANEL_H = 122;
constexpr int16_t LEVEL_PANEL_X = 356;
constexpr int16_t LEVEL_PANEL_Y = 256;
constexpr int16_t LEVEL_PANEL_W = 104;
constexpr int16_t LEVEL_PANEL_H = 50;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr ThemePalette GAMEBOY_PALETTE = {
    rgb(224, 248, 208), rgb(136, 192, 112), rgb(52, 104, 86),
    rgb(8, 24, 32), rgb(52, 104, 86), rgb(52, 104, 86),
    rgb(52, 104, 86), rgb(52, 104, 86), rgb(52, 104, 86),
    rgb(52, 104, 86), rgb(52, 104, 86), rgb(52, 104, 86),
    rgb(52, 104, 86), rgb(136, 192, 112)
};

constexpr ThemePalette MODERN_PALETTE = {
    TFT_BLACK, rgb(10, 30, 36), rgb(32, 68, 78),
    TFT_WHITE, rgb(128, 160, 168), TFT_CYAN,
    TFT_CYAN, TFT_YELLOW, TFT_PURPLE,
    TFT_GREEN, TFT_RED, TFT_BLUE,
    TFT_ORANGE, rgb(60, 90, 96)
};

// NES Tetris stores 10 tetrimino palettes and indexes them by level % 10.
constexpr uint16_t NES_LEVEL_COLORS[10][2] = {
    {rgb(0, 90, 156),   rgb(180, 49, 32)},
    {rgb(88, 40, 188),  rgb(216, 66, 48)},
    {rgb(0, 126, 136),  rgb(128, 68, 0)},
    {rgb(128, 52, 160), rgb(180, 49, 32)},
    {rgb(168, 48, 50),  rgb(104, 74, 0)},
    {rgb(0, 96, 176),   rgb(168, 48, 50)},
    {rgb(88, 88, 88),   rgb(0, 108, 0)},
    {rgb(104, 74, 0),   rgb(188, 70, 0)},
    {rgb(0, 108, 0),    rgb(180, 49, 32)},
    {rgb(64, 94, 220),  rgb(0, 108, 0)}
};

ThemePalette paletteFor(uint8_t level) {
    if (renderConfig.theme == ThemeId::Gameboy) {
        return GAMEBOY_PALETTE;
    }

    if (renderConfig.theme == ThemeId::Modern) {
        return MODERN_PALETTE;
    }

    const uint8_t index = level % 10;
    const uint16_t cool = NES_LEVEL_COLORS[index][0];
    const uint16_t warm = NES_LEVEL_COLORS[index][1];

    return {
        TFT_BLACK, rgb(36, 42, 40), rgb(96, 110, 104),
        TFT_WHITE, rgb(180, 190, 184), warm,
        cool, warm, cool, warm, cool, warm, cool,
        rgb(72, 82, 78)
    };
}

uint16_t colorForPiece(TetrominoType type, const ThemePalette& palette) {
    switch (type) {
        case TetrominoType::I: return palette.pieceI;
        case TetrominoType::O: return palette.pieceO;
        case TetrominoType::T: return palette.pieceT;
        case TetrominoType::S: return palette.pieceS;
        case TetrominoType::Z: return palette.pieceZ;
        case TetrominoType::J: return palette.pieceJ;
        case TetrominoType::L: return palette.pieceL;
        case TetrominoType::None:
            break;
    }

    return palette.panel;
}

void drawCell(int16_t x, int16_t y, int16_t size, uint16_t color, const ThemePalette& palette) {
    tft.fillRect(x + 1, y + 1, size - 2, size - 2, color);
    tft.drawRect(x, y, size, size, palette.grid);
}

void drawPiecePreview(TetrominoType type, int16_t x, int16_t y, int16_t cellSize, const ThemePalette& palette) {
    const uint16_t color = colorForPiece(type, palette);

    for (int8_t py = 0; py < 4; ++py) {
        for (int8_t px = 0; px < 4; ++px) {
            if (Tetromino::hasCell(type, 0, px, py)) {
                drawCell(x + px * cellSize, y + py * cellSize, cellSize, color, palette);
            }
        }
    }
}

void drawPanel(int16_t x, int16_t y, int16_t w, int16_t h, const char* title, const ThemePalette& palette) {
    tft.fillRect(x, y, w, h, palette.panel);
    tft.drawRect(x, y, w, h, palette.accent);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString(title, x + w / 2, y + 6, 2);
}

void drawTitle(const char* title, const ThemePalette& palette) {
    tft.fillScreen(palette.background);
    tft.setTextColor(palette.text, palette.background);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(title, tft.width() / 2, 10, 4);
}

bool renderCellsEqual(const RenderCell& a, const RenderCell& b) {
    return a.type == b.type && a.ghost == b.ghost;
}

void overlayPiece(RenderCell cells[BOARD_HEIGHT][BOARD_WIDTH], const ActivePieceState& piece, bool ghost) {
    if (piece.type == TetrominoType::None) {
        return;
    }

    for (int8_t py = 0; py < 4; ++py) {
        for (int8_t px = 0; px < 4; ++px) {
            if (!Tetromino::hasCell(piece.type, piece.rotation, px, py)) {
                continue;
            }

            const int8_t boardCol = piece.x + px;
            const int8_t boardRow = piece.y + py;
            if (boardCol < 0 || boardCol >= BOARD_WIDTH || boardRow < 0 || boardRow >= BOARD_HEIGHT) {
                continue;
            }

            if (ghost) {
                if (cells[boardRow][boardCol].type == TetrominoType::None) {
                    cells[boardRow][boardCol].ghost = true;
                }
            } else {
                cells[boardRow][boardCol] = {piece.type, false};
            }
        }
    }
}

void composeBoardCells(const PlayerRenderState& player, RenderCell cells[BOARD_HEIGHT][BOARD_WIDTH]) {
    for (uint8_t row = 0; row < BOARD_HEIGHT; ++row) {
        for (uint8_t col = 0; col < BOARD_WIDTH; ++col) {
            cells[row][col] = {
                static_cast<TetrominoType>(player.board.cells[row][col]),
                false
            };
        }
    }

    if (renderConfig.ghostEnabled && player.hasGhostPiece) {
        overlayPiece(cells, player.ghostPiece, true);
    }

    overlayPiece(cells, player.activePiece, false);
}

void drawBoardCell(
    uint8_t row,
    uint8_t col,
    const RenderCell& cell,
    const BoardLayout& layout,
    const ThemePalette& palette
) {
    const int16_t x = layout.boardX + col * layout.cellSize;
    const int16_t y = layout.boardY + row * layout.cellSize;
    const uint16_t color = cell.type == TetrominoType::None
        ? palette.background
        : colorForPiece(cell.type, palette);

    drawCell(x, y, layout.cellSize, color, palette);

    if (cell.ghost && cell.type == TetrominoType::None) {
        tft.drawRect(
            x + 2,
            y + 2,
            layout.cellSize - 4,
            layout.cellSize - 4,
            palette.ghost
        );
    }
}

void drawBoardFrame(const BoardLayout& layout, const ThemePalette& palette) {
    const int16_t boardW = BOARD_WIDTH * layout.cellSize;
    const int16_t boardH = BOARD_HEIGHT * layout.cellSize;

    tft.fillRect(layout.boardX, layout.boardY, boardW, boardH, palette.background);
    tft.drawRect(layout.boardX - 2, layout.boardY - 2, boardW + 4, boardH + 4, palette.accent);
}

void drawBoardFull(const PlayerRenderState& player, const BoardLayout& layout, const ThemePalette& palette) {
    RenderCell cells[BOARD_HEIGHT][BOARD_WIDTH];
    composeBoardCells(player, cells);
    drawBoardFrame(layout, palette);

    tft.startWrite();
    for (uint8_t row = 0; row < BOARD_HEIGHT; ++row) {
        for (uint8_t col = 0; col < BOARD_WIDTH; ++col) {
            drawBoardCell(row, col, cells[row][col], layout, palette);
        }
    }
    tft.endWrite();
}

void drawBoardDelta(
    const PlayerRenderState& previous,
    const PlayerRenderState& current,
    const BoardLayout& layout,
    const ThemePalette& palette
) {
    RenderCell previousCells[BOARD_HEIGHT][BOARD_WIDTH];
    RenderCell currentCells[BOARD_HEIGHT][BOARD_WIDTH];
    composeBoardCells(previous, previousCells);
    composeBoardCells(current, currentCells);

    tft.startWrite();
    for (uint8_t row = 0; row < BOARD_HEIGHT; ++row) {
        for (uint8_t col = 0; col < BOARD_WIDTH; ++col) {
            if (!renderCellsEqual(previousCells[row][col], currentCells[row][col])) {
                drawBoardCell(row, col, currentCells[row][col], layout, palette);
            }
        }
    }
    tft.endWrite();
}

void drawHoldPanel(const ThemePalette& palette) {
    if (!renderConfig.holdEnabled) {
        return;
    }

    drawPanel(HOLD_PANEL_X, HOLD_PANEL_Y, HOLD_PANEL_W, HOLD_PANEL_H, "HOLD", palette);
}

void clearHoldPreview(const ThemePalette& palette) {
    tft.fillRect(HOLD_PREVIEW_X - 8, HOLD_PREVIEW_Y - 4, 56, 44, palette.panel);
}

void drawHoldPreview(TetrominoType holdPiece, const ThemePalette& palette) {
    if (!renderConfig.holdEnabled) {
        return;
    }

    clearHoldPreview(palette);
    if (holdPiece != TetrominoType::None) {
        drawPiecePreview(holdPiece, HOLD_PREVIEW_X, HOLD_PREVIEW_Y, PREVIEW_CELL_SIZE, palette);
    }
}

uint8_t visibleNextCount(const PlayerRenderState& player) {
    return min(player.nextPieceCount, min(renderConfig.nextPreviewCount, MAX_NEXT_PIECES));
}

void drawNextPanel(const PlayerRenderState& player, const ThemePalette& palette) {
    const uint8_t previewCount = min(player.nextPieceCount, min(renderConfig.nextPreviewCount, MAX_NEXT_PIECES));
    if (previewCount == 0) {
        return;
    }

    drawPanel(NEXT_PANEL_X, NEXT_PANEL_Y, NEXT_PANEL_W, NEXT_PANEL_H, "NEXT", palette);
}

void clearNextPreview(const ThemePalette& palette) {
    tft.fillRect(NEXT_PREVIEW_X - 10, NEXT_PREVIEW_Y - 4, 72, 174, palette.panel);
}

void drawNextPreview(const PlayerRenderState& player, const ThemePalette& palette) {
    const uint8_t previewCount = visibleNextCount(player);
    if (previewCount == 0) {
        return;
    }

    clearNextPreview(palette);
    for (uint8_t i = 0; i < previewCount; ++i) {
        drawPiecePreview(player.nextPieces[i], NEXT_PREVIEW_X, NEXT_PREVIEW_Y + i * 34, 8, palette);
    }
}

bool nextPreviewChanged(const PlayerRenderState& previous, const PlayerRenderState& current) {
    const uint8_t previousCount = visibleNextCount(previous);
    const uint8_t currentCount = visibleNextCount(current);
    if (previousCount != currentCount) {
        return true;
    }

    for (uint8_t i = 0; i < currentCount; ++i) {
        if (previous.nextPieces[i] != current.nextPieces[i]) {
            return true;
        }
    }

    return false;
}

void drawStatsPanels(const ThemePalette& palette) {
    drawPanel(STATS_PANEL_X, STATS_PANEL_Y, STATS_PANEL_W, STATS_PANEL_H, "STATS", palette);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString("Score", 30, 158, 2);
    tft.drawString("Lines", 30, 204, 2);

    drawPanel(LEVEL_PANEL_X, LEVEL_PANEL_Y, LEVEL_PANEL_W, LEVEL_PANEL_H, "LEVEL", palette);
}

void drawScoreValue(uint32_t score, const ThemePalette& palette) {
    tft.fillRect(28, 176, 84, 20, palette.panel);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawNumber(score, 30, 178, 2);
}

void drawLinesValue(uint16_t lines, const ThemePalette& palette) {
    tft.fillRect(28, 222, 84, 20, palette.panel);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawNumber(lines, 30, 224, 2);
}

void drawLevelValue(uint8_t level, const ThemePalette& palette) {
    tft.fillRect(370, 280, 76, 18, palette.panel);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawNumber(level, 408, 282, 2);
}

void drawStatValues(const PlayerRenderState& player, const ThemePalette& palette) {
    drawScoreValue(player.score, palette);
    drawLinesValue(player.linesCleared, palette);
    drawLevelValue(player.level, palette);
}

void drawGameplayChrome(const PlayerRenderState& player, const ThemePalette& palette) {
    drawTitle("Tetris Battle", palette);
    drawHoldPanel(palette);
    drawNextPanel(player, palette);
    drawStatsPanels(palette);
}

TetrominoType effectiveHoldPiece(const PlayerRenderState& player) {
    return player.hasHoldPiece ? player.holdPiece : TetrominoType::None;
}

bool needsFullGameplayRedraw(const PlayerRenderState& player) {
    if (!gameplayCache.valid || !hasCurrentScreen || currentScreen != RenderScreen::Gameplay) {
        return true;
    }

    return renderConfig.theme == ThemeId::Nintendo && gameplayCache.player.level != player.level;
}

void drawGameplayFull(const PlayerRenderState& player) {
    const ThemePalette palette = paletteFor(player.level);

    drawGameplayChrome(player, palette);
    drawBoardFull(player, GAMEPLAY_LAYOUT, palette);
    tft.startWrite();
    drawHoldPreview(effectiveHoldPiece(player), palette);
    drawNextPreview(player, palette);
    drawStatValues(player, palette);
    tft.endWrite();
}

void drawGameplayDelta(const PlayerRenderState& previous, const PlayerRenderState& current) {
    const ThemePalette palette = paletteFor(current.level);

    drawBoardDelta(previous, current, GAMEPLAY_LAYOUT, palette);

    tft.startWrite();
    if (effectiveHoldPiece(previous) != effectiveHoldPiece(current)) {
        drawHoldPreview(effectiveHoldPiece(current), palette);
    }

    if (nextPreviewChanged(previous, current)) {
        drawNextPreview(current, palette);
    }

    if (previous.score != current.score) {
        drawScoreValue(current.score, palette);
    }

    if (previous.linesCleared != current.linesCleared) {
        drawLinesValue(current.linesCleared, palette);
    }

    if (previous.level != current.level) {
        drawLevelValue(current.level, palette);
    }
    tft.endWrite();
}

void renderGameplay(const GameplayRenderState& gameplay) {
    if (needsFullGameplayRedraw(gameplay.player)) {
        drawGameplayFull(gameplay.player);
    } else {
        drawGameplayDelta(gameplayCache.player, gameplay.player);
    }

    gameplayCache.player = gameplay.player;
    gameplayCache.valid = true;
    currentScreen = RenderScreen::Gameplay;
    hasCurrentScreen = true;
}

void drawLobby(const LobbyRenderState& lobby) {
    const ThemePalette palette = paletteFor(0);
    drawTitle("Lobby", palette);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(palette.text, palette.background);
    tft.drawString("Local", 42, 86, 2);
    tft.drawString(lobby.localUsername, 42, 112, 4);
    tft.setTextColor(lobby.localReady ? TFT_GREEN : TFT_ORANGE, palette.background);
    tft.drawString(lobby.localReady ? "Ready" : "Not ready", 42, 150, 2);

    tft.setTextColor(palette.text, palette.background);
    tft.drawString("Remote", 250, 86, 2);
    tft.drawString(lobby.remoteUsername, 250, 112, 4);
    tft.setTextColor(lobby.remoteReady ? TFT_GREEN : TFT_ORANGE, palette.background);
    tft.drawString(lobby.remoteReady ? "Ready" : "Not ready", 250, 150, 2);

    currentScreen = RenderScreen::Lobby;
    hasCurrentScreen = true;
}

void drawPaused(const PauseRenderState& pause) {
    const ThemePalette palette = paletteFor(pause.player.level);
    drawGameplayFull(pause.player);
    gameplayCache.player = pause.player;
    gameplayCache.valid = true;

    tft.fillRect(142, 116, 196, 88, palette.panel);
    tft.drawRect(142, 116, 196, 88, palette.accent);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString("Paused", 240, 132, 4);

    if (pause.showPauseMenu && pause.isPauseOwner) {
        tft.drawString("Resume  Restart  Quit", 240, 174, 2);
    } else {
        tft.drawString("Waiting", 240, 174, 2);
    }

    currentScreen = RenderScreen::Paused;
    hasCurrentScreen = true;
}

void drawGameOver(const GameOverRenderState& gameOver) {
    const ThemePalette palette = paletteFor(gameOver.player.level);
    drawGameplayFull(gameOver.player);
    gameplayCache.player = gameOver.player;
    gameplayCache.valid = true;

    tft.fillRect(130, 112, 220, 96, palette.panel);
    tft.drawRect(130, 112, 220, 96, TFT_RED);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString("Game Over", 240, 130, 4);
    tft.drawString("Final score", 240, 170, 2);
    tft.drawNumber(gameOver.player.score, 240, 190, 2);

    currentScreen = RenderScreen::GameOver;
    hasCurrentScreen = true;
}

}  // namespace

namespace Display {

bool begin() {
    if (initialized) {
        return true;
    }

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(paletteFor(0).background);

    initialized = true;
    return true;
}

void configure(const DisplayRenderConfig& config) {
    if (!initialized && !begin()) {
        return;
    }

    renderConfig = config;
    renderConfig.nextPreviewCount = min(renderConfig.nextPreviewCount, MAX_NEXT_PIECES);
}

void renderScreen(const ScreenRenderState& screen) {
    if (!initialized && !begin()) {
        return;
    }

    switch (screen.screen) {
        case RenderScreen::Lobby:
            drawLobby(screen.payload.lobby);
            break;
        case RenderScreen::Gameplay:
            renderGameplay(screen.payload.gameplay);
            break;
        case RenderScreen::Paused:
            drawPaused(screen.payload.pause);
            break;
        case RenderScreen::GameOver:
            drawGameOver(screen.payload.gameOver);
            break;
    }
}

}  // namespace Display
