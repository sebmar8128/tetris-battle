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

TFT_eSPI tft;
bool initialized = false;

DisplayRenderConfig renderConfig = {
    ThemeId::Gameboy,
    true,
    true,
    MAX_NEXT_PIECES
};

constexpr BoardLayout GAMEPLAY_LAYOUT = {150, 34, 12};
constexpr int16_t PREVIEW_CELL_SIZE = 10;

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

void drawBoard(const PlayerRenderState& player, const BoardLayout& layout, const ThemePalette& palette) {
    const int16_t boardW = BOARD_WIDTH * layout.cellSize;
    const int16_t boardH = BOARD_HEIGHT * layout.cellSize;

    tft.fillRect(layout.boardX, layout.boardY, boardW, boardH, palette.background);
    tft.drawRect(layout.boardX - 2, layout.boardY - 2, boardW + 4, boardH + 4, palette.accent);

    for (uint8_t row = 0; row < BOARD_HEIGHT; ++row) {
        for (uint8_t col = 0; col < BOARD_WIDTH; ++col) {
            const TetrominoType cellType = static_cast<TetrominoType>(player.board.cells[row][col]);
            const uint16_t color = cellType == TetrominoType::None
                ? palette.background
                : colorForPiece(cellType, palette);
            drawCell(
                layout.boardX + col * layout.cellSize,
                layout.boardY + row * layout.cellSize,
                layout.cellSize,
                color,
                palette
            );
        }
    }

    if (renderConfig.ghostEnabled && player.hasGhostPiece) {
        const ActivePieceState& ghost = player.ghostPiece;
        for (int8_t py = 0; py < 4; ++py) {
            for (int8_t px = 0; px < 4; ++px) {
                if (Tetromino::hasCell(ghost.type, ghost.rotation, px, py)) {
                    const int8_t boardCol = ghost.x + px;
                    const int8_t boardRow = ghost.y + py;
                    if (boardCol >= 0 && boardCol < BOARD_WIDTH && boardRow >= 0 && boardRow < BOARD_HEIGHT) {
                        tft.drawRect(
                            layout.boardX + boardCol * layout.cellSize + 2,
                            layout.boardY + boardRow * layout.cellSize + 2,
                            layout.cellSize - 4,
                            layout.cellSize - 4,
                            palette.ghost
                        );
                    }
                }
            }
        }
    }

    const ActivePieceState& active = player.activePiece;
    for (int8_t py = 0; py < 4; ++py) {
        for (int8_t px = 0; px < 4; ++px) {
            if (Tetromino::hasCell(active.type, active.rotation, px, py)) {
                const int8_t boardCol = active.x + px;
                const int8_t boardRow = active.y + py;
                if (boardCol >= 0 && boardCol < BOARD_WIDTH && boardRow >= 0 && boardRow < BOARD_HEIGHT) {
                    drawCell(
                        layout.boardX + boardCol * layout.cellSize,
                        layout.boardY + boardRow * layout.cellSize,
                        layout.cellSize,
                        colorForPiece(active.type, palette),
                        palette
                    );
                }
            }
        }
    }
}

void drawHoldWindow(const PlayerRenderState& player, const ThemePalette& palette) {
    if (!renderConfig.holdEnabled) {
        return;
    }

    drawPanel(18, 34, 104, 74, "HOLD", palette);
    if (player.hasHoldPiece) {
        drawPiecePreview(player.holdPiece, 48, 60, PREVIEW_CELL_SIZE, palette);
    }
}

void drawNextWindow(const PlayerRenderState& player, const ThemePalette& palette) {
    const uint8_t previewCount = min(player.nextPieceCount, min(renderConfig.nextPreviewCount, MAX_NEXT_PIECES));
    if (previewCount == 0) {
        return;
    }

    drawPanel(356, 34, 104, 210, "NEXT", palette);
    for (uint8_t i = 0; i < previewCount; ++i) {
        drawPiecePreview(player.nextPieces[i], 386, 62 + i * 34, 8, palette);
    }
}

void drawPlayerStats(const PlayerRenderState& player, const ThemePalette& palette) {
    drawPanel(18, 130, 104, 122, "STATS", palette);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString("Score", 30, 158, 2);
    tft.drawNumber(player.score, 30, 178, 2);
    tft.drawString("Lines", 30, 204, 2);
    tft.drawNumber(player.linesCleared, 30, 224, 2);

    drawPanel(356, 256, 104, 50, "LEVEL", palette);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawNumber(player.level, 408, 282, 2);
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
}

void drawGameplay(const GameplayRenderState& gameplay) {
    const ThemePalette palette = paletteFor(gameplay.player.level);
    drawTitle("Tetris Battle", palette);
    drawHoldWindow(gameplay.player, palette);
    drawNextWindow(gameplay.player, palette);
    drawBoard(gameplay.player, GAMEPLAY_LAYOUT, palette);
    drawPlayerStats(gameplay.player, palette);
}

void drawPaused(const PauseRenderState& pause) {
    const ThemePalette palette = paletteFor(pause.player.level);
    drawGameplay({pause.player});

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
}

void drawGameOver(const GameOverRenderState& gameOver) {
    const ThemePalette palette = paletteFor(gameOver.player.level);
    drawGameplay({gameOver.player});

    tft.fillRect(130, 112, 220, 96, palette.panel);
    tft.drawRect(130, 112, 220, 96, TFT_RED);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString("Game Over", 240, 130, 4);
    tft.drawString("Final score", 240, 170, 2);
    tft.drawNumber(gameOver.player.score, 240, 190, 2);
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
            drawGameplay(screen.payload.gameplay);
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
