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
    uint16_t garbage;
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
    bool remoteFinished;
};

struct LobbyRenderCache {
    bool valid;
    LobbyRenderState lobby;
};

TFT_eSPI tft;
bool initialized = false;
bool hasCurrentScreen = false;
RenderScreen currentScreen = RenderScreen::Gameplay;
GameplayRenderCache gameplayCache = {};
LobbyRenderCache lobbyCache = {};

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
constexpr int16_t LOBBY_PROFILE_W = 136;
constexpr int16_t LOBBY_PROFILE_H = 128;
constexpr int16_t LOBBY_LEFT_X = 24;
constexpr int16_t LOBBY_RIGHT_X = 320;
constexpr int16_t LOBBY_TOP_Y = 44;
constexpr int16_t LOBBY_MATCH_X = 56;
constexpr int16_t LOBBY_MATCH_Y = 188;
constexpr int16_t LOBBY_MATCH_W = 368;
constexpr int16_t LOBBY_MATCH_H = 112;
constexpr int16_t LOBBY_MATCH_LABEL_X = 86;
constexpr int16_t LOBBY_MATCH_VALUE_X = 288;
constexpr uint8_t LOBBY_NAME_FONT = 4;
constexpr uint8_t LOBBY_NAME_FALLBACK_FONT = 2;
constexpr uint8_t LOBBY_STATUS_FONT = 2;
constexpr uint8_t LOBBY_MATCH_FONT = 2;
constexpr int16_t LOBBY_BG_CELL_SIZE = 12;
constexpr int16_t LOBBY_BG_STRIDE_X = 44;
constexpr int16_t LOBBY_BG_STRIDE_Y = 36;
constexpr int16_t LOBBY_BG_OFFSET_X = 6;
constexpr int16_t LOBBY_BG_OFFSET_Y = 20;
constexpr TetrominoType LOBBY_BG_TYPES[] = {
    TetrominoType::I,
    TetrominoType::O,
    TetrominoType::T,
    TetrominoType::S,
    TetrominoType::Z,
    TetrominoType::J,
    TetrominoType::L
};

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr ThemePalette GAMEBOY_PALETTE = {
    rgb(224, 248, 208), rgb(136, 192, 112), rgb(52, 104, 86),
    rgb(8, 24, 32), rgb(52, 104, 86), rgb(52, 104, 86),
    rgb(52, 104, 86), rgb(52, 104, 86), rgb(52, 104, 86),
    rgb(52, 104, 86), rgb(52, 104, 86), rgb(52, 104, 86),
    rgb(52, 104, 86), rgb(8, 24, 32), rgb(136, 192, 112)
};

constexpr ThemePalette MODERN_PALETTE = {
    TFT_BLACK, rgb(10, 30, 36), rgb(32, 68, 78),
    TFT_WHITE, rgb(128, 160, 168), TFT_CYAN,
    TFT_CYAN, TFT_YELLOW, TFT_PURPLE,
    TFT_GREEN, TFT_RED, TFT_BLUE,
    TFT_ORANGE, rgb(96, 104, 108), rgb(60, 90, 96)
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
        rgb(132, 144, 138),
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
        case TetrominoType::Garbage: return palette.garbage;
        case TetrominoType::None:
            break;
    }

    return palette.panel;
}

void drawCell(int16_t x, int16_t y, int16_t size, uint16_t color, const ThemePalette& palette) {
    tft.fillRect(x + 1, y + 1, size - 2, size - 2, color);
    tft.drawRect(x, y, size, size, palette.grid);
}

void drawPieceSilhouette(
    TetrominoType type,
    uint8_t rotation,
    int16_t x,
    int16_t y,
    int16_t cellSize,
    const ThemePalette& palette
) {
    const uint16_t color = colorForPiece(type, palette);
    for (int8_t py = 0; py < 4; ++py) {
        for (int8_t px = 0; px < 4; ++px) {
            if (!Tetromino::hasCell(type, rotation, px, py)) {
                continue;
            }

            drawCell(x + px * cellSize, y + py * cellSize, cellSize, color, palette);
        }
    }
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

void drawLobbyBackground(const ThemePalette& palette) {
    tft.fillScreen(palette.background);

    const int16_t width = tft.width();
    const int16_t height = tft.height();
    const uint8_t typeCount = sizeof(LOBBY_BG_TYPES) / sizeof(LOBBY_BG_TYPES[0]);

    for (int16_t y = LOBBY_BG_OFFSET_Y, row = 0; y < height; y += LOBBY_BG_STRIDE_Y, ++row) {
        for (int16_t x = LOBBY_BG_OFFSET_X + ((row & 1) ? 18 : 0), col = 0;
             x < width;
             x += LOBBY_BG_STRIDE_X, ++col) {
            const TetrominoType type = LOBBY_BG_TYPES[(row + col) % typeCount];
            const uint8_t rotation = static_cast<uint8_t>((row * 3 + col) % Tetromino::ROTATION_COUNT);
            drawPieceSilhouette(type, rotation, x, y, LOBBY_BG_CELL_SIZE, palette);
        }
    }
}

const char* gameModeLabel(GameMode mode) {
    switch (mode) {
        case GameMode::Marathon: return "Marathon";
        case GameMode::Sprint40: return "Sprint 40";
    }

    return "";
}

const char* lobbyStatusLabel(LobbyPeerStatus status) {
    switch (status) {
        case LobbyPeerStatus::Offline: return "Offline";
        case LobbyPeerStatus::Online: return "Online";
        case LobbyPeerStatus::InLobby: return "In Lobby";
    }

    return "";
}

uint16_t lobbyStatusColor(LobbyPeerStatus status) {
    switch (status) {
        case LobbyPeerStatus::Offline: return TFT_RED;
        case LobbyPeerStatus::Online: return TFT_ORANGE;
        case LobbyPeerStatus::InLobby: return TFT_GREEN;
    }

    return TFT_WHITE;
}

const char* pauseActionLabel(PauseMenuAction action) {
    switch (action) {
        case PauseMenuAction::Resume: return "Resume";
        case PauseMenuAction::Restart: return "Restart";
        case PauseMenuAction::Quit: return "Quit";
    }

    return "";
}

const char* postGameActionLabel(PostGameMenuAction action) {
    switch (action) {
        case PostGameMenuAction::Restart: return "Restart";
        case PostGameMenuAction::Quit: return "Quit";
    }

    return "";
}

const char* gameOverReasonLabel(GameOverReason reason) {
    switch (reason) {
        case GameOverReason::TopOut: return "Top Out";
        case GameOverReason::GarbageCrush: return "Garbage Crush";
        case GameOverReason::SprintComplete: return "Sprint Complete";
        case GameOverReason::Quit: return "Quit";
        case GameOverReason::Disconnect: return "Disconnect";
    }

    return "";
}

void drawCenteredPanel(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    const char* title,
    const ThemePalette& palette
) {
    tft.fillRect(x, y, w, h, palette.panel);
    tft.drawRect(x, y, w, h, palette.accent);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString(title, x + w / 2, y + 10, 4);
}

void drawHorizontalChoice(
    int16_t centerX,
    int16_t y,
    const char* leftLabel,
    bool leftSelected,
    const char* rightLabel,
    bool rightSelected,
    const ThemePalette& palette
) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(leftSelected ? palette.accent : palette.text, palette.panel);
    tft.drawString(leftLabel, centerX - 58, y, 2);
    tft.setTextColor(rightSelected ? palette.accent : palette.text, palette.panel);
    tft.drawString(rightLabel, centerX + 58, y, 2);
}

void drawCenteredBodyText(const char* line, int16_t x, int16_t y, const ThemePalette& palette) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString(line, x, y, 2);
}

uint8_t chooseBuiltinFontToFit(const char* text, int16_t maxWidth, uint8_t preferred, uint8_t fallback) {
    if (tft.textWidth(text, preferred) <= maxWidth) {
        return preferred;
    }

    return fallback;
}

void drawCenteredBuiltinText(
    const char* text,
    int16_t centerX,
    int16_t y,
    uint16_t color,
    uint16_t background,
    uint8_t font
) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(color, background);
    tft.drawString(text, centerX, y, font);
}

void drawLeftBuiltinText(
    const char* text,
    int16_t x,
    int16_t y,
    uint16_t color,
    uint16_t background,
    uint8_t font
) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(color, background);
    tft.drawString(text, x, y, font);
}

void hardResetDisplay() {
#ifdef TFT_RST
    if (TFT_RST >= 0) {
        pinMode(TFT_RST, OUTPUT);
        digitalWrite(TFT_RST, LOW);
        delay(80);
        digitalWrite(TFT_RST, HIGH);
        delay(180);
    }
#endif
}

bool renderCellsEqual(const RenderCell& a, const RenderCell& b) {
    return a.type == b.type && a.ghost == b.ghost;
}

bool stringsEqual(const char* a, const char* b) {
    return strncmp(a, b, MAX_USERNAME_LEN + 1) == 0;
}

bool matchSettingsEqual(const MatchSettings& a, const MatchSettings& b) {
    return a.garbageEnabled == b.garbageEnabled &&
           a.mode == b.mode &&
           a.startingLevel == b.startingLevel;
}

bool lobbyProfileChanged(const LobbyRenderState& previous, const LobbyRenderState& current, bool local) {
    if (local) {
        return !stringsEqual(previous.localUsername, current.localUsername);
    }

    return !stringsEqual(previous.remoteUsername, current.remoteUsername) ||
           previous.remoteStatus != current.remoteStatus;
}

bool lobbyMatchPanelChanged(const LobbyRenderState& previous, const LobbyRenderState& current) {
    return !matchSettingsEqual(previous.matchSettings, current.matchSettings) ||
           previous.selectedItem != current.selectedItem ||
           previous.remoteStatus != current.remoteStatus;
}

bool lobbyModalChanged(const LobbyRenderState& previous, const LobbyRenderState& current) {
    return previous.modalState != current.modalState ||
           previous.incomingStartRequest != current.incomingStartRequest ||
           previous.confirmAcceptSelected != current.confirmAcceptSelected;
}

int16_t lobbyMatchRowY(uint8_t row) {
    return 216 + row * 20;
}

bool lobbyMatchRowValueChanged(const LobbyRenderState& previous, const LobbyRenderState& current, uint8_t row) {
    switch (row) {
        case 0:
            return previous.matchSettings.garbageEnabled != current.matchSettings.garbageEnabled;
        case 1:
            return previous.matchSettings.mode != current.matchSettings.mode;
        case 2:
            return previous.matchSettings.startingLevel != current.matchSettings.startingLevel;
        case 3:
            return previous.remoteStatus != current.remoteStatus;
    }

    return false;
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

    if (cell.type == TetrominoType::Garbage) {
        tft.drawLine(x + 3, y + 3, x + layout.cellSize - 4, y + layout.cellSize - 4, palette.grid);
        tft.drawLine(x + layout.cellSize - 4, y + 3, x + 3, y + layout.cellSize - 4, palette.grid);
    }

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

bool needsFullGameplayRedraw(const GameplayRenderState& gameplay) {
    if (!gameplayCache.valid || !hasCurrentScreen || currentScreen != RenderScreen::Gameplay) {
        return true;
    }

    return gameplayCache.remoteFinished != gameplay.remoteFinished ||
           (renderConfig.theme == ThemeId::Nintendo && gameplayCache.player.level != gameplay.player.level);
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
    if (needsFullGameplayRedraw(gameplay)) {
        drawGameplayFull(gameplay.player);
    } else {
        drawGameplayDelta(gameplayCache.player, gameplay.player);
    }

    if (gameplay.remoteFinished) {
        const ThemePalette palette = paletteFor(gameplay.player.level);
        tft.fillRect(156, 8, 168, 22, palette.panel);
        tft.drawRect(156, 8, 168, 22, TFT_ORANGE);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_ORANGE, palette.panel);
        tft.drawString("Opponent Out", 240, 12, 2);
    }

    gameplayCache.player = gameplay.player;
    gameplayCache.remoteFinished = gameplay.remoteFinished;
    gameplayCache.valid = true;
    currentScreen = RenderScreen::Gameplay;
    hasCurrentScreen = true;
}

void drawLobbyProfileCard(int16_t x, const char* title, const char* username, const char* status, uint16_t statusColor, const ThemePalette& palette) {
    drawPanel(x, LOBBY_TOP_Y, LOBBY_PROFILE_W, LOBBY_PROFILE_H, title, palette);

    const int16_t centerX = x + LOBBY_PROFILE_W / 2;
    const uint8_t nameFont = chooseBuiltinFontToFit(
        username,
        LOBBY_PROFILE_W - 18,
        LOBBY_NAME_FONT,
        LOBBY_NAME_FALLBACK_FONT
    );

    drawCenteredBuiltinText(
        username,
        centerX,
        nameFont == LOBBY_NAME_FONT ? 92 : 102,
        palette.text,
        palette.panel,
        nameFont
    );
    drawCenteredBuiltinText(
        status,
        centerX,
        132,
        statusColor,
        palette.panel,
        LOBBY_STATUS_FONT
    );
}

void drawLobbyMatchPanelFrame(const ThemePalette& palette) {
    drawPanel(LOBBY_MATCH_X, LOBBY_MATCH_Y, LOBBY_MATCH_W, LOBBY_MATCH_H, "MATCH", palette);
}

void drawLobbyMatchRow(const LobbyRenderState& lobby, uint8_t row, const ThemePalette& palette) {
    const char* labels[] = {
        "Garbage",
        "Mode",
        "Start level",
        "Start game"
    };

    const char* values[] = {
        lobby.matchSettings.garbageEnabled ? "On" : "Off",
        gameModeLabel(lobby.matchSettings.mode),
        "",
        lobby.remoteStatus == LobbyPeerStatus::InLobby ? "Ready" : "Peer not ready"
    };

    const int16_t y = lobbyMatchRowY(row);
    const bool selected = static_cast<uint8_t>(lobby.selectedItem) == row && lobby.modalState == ModalState::None;
    const uint16_t color = selected ? palette.accent : palette.text;
    tft.fillRect(LOBBY_MATCH_X + 2, y - 2, LOBBY_MATCH_W - 4, 18, palette.panel);

    drawLeftBuiltinText(
        labels[row],
        LOBBY_MATCH_LABEL_X,
        y,
        color,
        palette.panel,
        LOBBY_MATCH_FONT
    );

    if (row == 2) {
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(color, palette.panel);
        tft.drawNumber(lobby.matchSettings.startingLevel, LOBBY_MATCH_VALUE_X, y, LOBBY_MATCH_FONT);
    } else {
        drawLeftBuiltinText(
            values[row],
            LOBBY_MATCH_VALUE_X,
            y,
            color,
            palette.panel,
            LOBBY_MATCH_FONT
        );
    }
}

void drawLobbyMatchPanel(const LobbyRenderState& lobby, const ThemePalette& palette) {
    drawLobbyMatchPanelFrame(palette);
    for (uint8_t row = 0; row < 4; ++row) {
        drawLobbyMatchRow(lobby, row, palette);
    }
}

void drawLobbyModal(const LobbyRenderState& lobby, const ThemePalette& palette) {
    if (lobby.modalState == ModalState::OutgoingRequest) {
        drawCenteredPanel(120, 88, 240, 108, "Start Game", palette);
        drawCenteredBodyText("Waiting for other player...", 240, 144, palette);
    } else if (lobby.modalState == ModalState::IncomingRequest) {
        drawCenteredPanel(120, 88, 240, 108, "Start Game", palette);
        drawCenteredBodyText("Accept start request?", 240, 136, palette);
        drawHorizontalChoice(
            240,
            170,
            "Yes",
            lobby.confirmAcceptSelected,
            "No",
            !lobby.confirmAcceptSelected,
            palette
        );
    }
}

void drawLobbyFull(const LobbyRenderState& lobby) {
    const ThemePalette palette = paletteFor(0);
    drawLobbyBackground(palette);
    tft.setTextColor(palette.text, palette.background);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Lobby", tft.width() / 2, 10, 4);
    drawLobbyProfileCard(LOBBY_LEFT_X, "YOU", lobby.localUsername, "In lobby", palette.text, palette);
    drawLobbyProfileCard(
        LOBBY_RIGHT_X,
        "PEER",
        lobby.remoteUsername,
        lobbyStatusLabel(lobby.remoteStatus),
        lobbyStatusColor(lobby.remoteStatus),
        palette
    );
    drawLobbyMatchPanel(lobby, palette);
    drawLobbyModal(lobby, palette);
}

void renderLobby(const LobbyRenderState& lobby) {
    const ThemePalette palette = paletteFor(0);
    const bool fullRedraw = !lobbyCache.valid ||
                            !hasCurrentScreen ||
                            currentScreen != RenderScreen::Lobby ||
                            lobbyModalChanged(lobbyCache.lobby, lobby);

    if (fullRedraw) {
        drawLobbyFull(lobby);
    } else {
        if (lobbyProfileChanged(lobbyCache.lobby, lobby, true)) {
            drawLobbyProfileCard(LOBBY_LEFT_X, "YOU", lobby.localUsername, "In lobby", palette.text, palette);
        }

        if (lobbyProfileChanged(lobbyCache.lobby, lobby, false)) {
            drawLobbyProfileCard(
                LOBBY_RIGHT_X,
                "PEER",
                lobby.remoteUsername,
                lobbyStatusLabel(lobby.remoteStatus),
                lobbyStatusColor(lobby.remoteStatus),
                palette
            );
        }

        if (lobbyMatchPanelChanged(lobbyCache.lobby, lobby)) {
            for (uint8_t row = 0; row < 4; ++row) {
                const bool selectionChanged = lobbyCache.lobby.selectedItem == static_cast<LobbyMenuItem>(row) ||
                                             lobby.selectedItem == static_cast<LobbyMenuItem>(row);
                if (selectionChanged || lobbyMatchRowValueChanged(lobbyCache.lobby, lobby, row)) {
                    drawLobbyMatchRow(lobby, row, palette);
                }
            }
        }
    }

    lobbyCache.lobby = lobby;
    lobbyCache.valid = true;

    currentScreen = RenderScreen::Lobby;
    hasCurrentScreen = true;
}

void drawPaused(const PauseRenderState& pause) {
    const ThemePalette palette = paletteFor(pause.player.level);
    drawGameplayFull(pause.player);
    gameplayCache.player = pause.player;
    gameplayCache.remoteFinished = false;
    gameplayCache.valid = true;

    drawCenteredPanel(108, 104, 264, 112, "Paused", palette);

    if (pause.panelState == PausePanelState::Menu) {
        drawHorizontalChoice(
            240,
            172,
            pause.selectedAction == PauseMenuAction::Resume ? "[Resume]" : "Resume",
            pause.selectedAction == PauseMenuAction::Resume,
            pause.selectedAction == PauseMenuAction::Restart ? "[Restart]" : "Restart",
            pause.selectedAction == PauseMenuAction::Restart,
            palette
        );
        tft.setTextColor(pause.selectedAction == PauseMenuAction::Quit ? palette.accent : palette.text, palette.panel);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("Quit", 240, 194, 2);
    } else if (pause.panelState == PausePanelState::OutgoingRequest) {
        drawCenteredBodyText("Waiting for other player...", 240, 156, palette);
        drawCenteredBodyText(pauseActionLabel(pause.pendingAction), 240, 182, palette);
    } else {
        drawCenteredBodyText("Accept request?", 240, 148, palette);
        drawCenteredBodyText(pauseActionLabel(pause.pendingAction), 240, 170, palette);
        drawHorizontalChoice(
            240,
            194,
            "Yes",
            pause.confirmAcceptSelected,
            "No",
            !pause.confirmAcceptSelected,
            palette
        );
    }

    currentScreen = RenderScreen::Paused;
    hasCurrentScreen = true;
}

void drawGameOver(const GameOverRenderState& gameOver) {
    const ThemePalette palette = paletteFor(gameOver.player.level);
    drawGameplayFull(gameOver.player);
    gameplayCache.player = gameOver.player;
    gameplayCache.remoteFinished = gameOver.remoteFinished;
    gameplayCache.valid = true;

    drawCenteredPanel(104, 84, 272, 152, "Game Over", palette);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_RED, palette.panel);
    tft.drawString(gameOverReasonLabel(gameOver.reason), 240, 132, 2);
    tft.setTextColor(palette.text, palette.panel);
    tft.drawString("Score", 240, 156, 2);
    tft.drawNumber(gameOver.player.score, 240, 176, 2);

    if (gameOver.panelState == GameOverPanelState::WaitingForRemote) {
        drawCenteredBodyText("Waiting for opponent...", 240, 202, palette);
    } else if (gameOver.panelState == GameOverPanelState::Disconnected) {
        drawCenteredBodyText("Peer disconnected", 240, 202, palette);
        tft.setTextColor(palette.accent, palette.panel);
        tft.drawString("Center: Quit", 240, 224, 2);
    } else if (gameOver.panelState == GameOverPanelState::Menu) {
        tft.setTextColor(gameOver.localWon ? TFT_GREEN : TFT_RED, palette.panel);
        tft.drawString(gameOver.localWon ? "YOU WIN" : "YOU LOSE", 240, 202, 2);
        drawHorizontalChoice(
            240,
            224,
            "Restart",
            gameOver.selectedAction == PostGameMenuAction::Restart,
            "Quit",
            gameOver.selectedAction == PostGameMenuAction::Quit,
            palette
        );
    } else if (gameOver.panelState == GameOverPanelState::OutgoingRequest) {
        drawCenteredBodyText("Waiting for other player...", 240, 204, palette);
        drawCenteredBodyText(postGameActionLabel(gameOver.pendingAction), 240, 224, palette);
    } else if (gameOver.panelState == GameOverPanelState::IncomingRequest) {
        drawCenteredBodyText("Accept request?", 240, 200, palette);
        drawCenteredBodyText(postGameActionLabel(gameOver.pendingAction), 240, 220, palette);
        drawHorizontalChoice(
            240,
            232,
            "Yes",
            gameOver.confirmAcceptSelected,
            "No",
            !gameOver.confirmAcceptSelected,
            palette
        );
    }

    currentScreen = RenderScreen::GameOver;
    hasCurrentScreen = true;
}

}  // namespace

namespace Display {

bool begin() {
    if (initialized) {
        return true;
    }

    hardResetDisplay();
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
    gameplayCache.valid = false;
    lobbyCache.valid = false;
    hasCurrentScreen = false;
}

void renderScreen(const ScreenRenderState& screen) {
    if (!initialized && !begin()) {
        return;
    }

    switch (screen.screen) {
        case RenderScreen::Lobby:
            renderLobby(screen.payload.lobby);
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
