#include "game_controller.h"

#include "default_settings.h"

void GameController::begin() {
    userSettings_ = DefaultSettings::userSettings();
    matchSettings_ = DefaultSettings::matchSettings();

    engine_.begin({
        DefaultSettings::GAME_SEED,
        matchSettings_,
        userSettings_
    });
}

bool GameController::handleInputEvent(const InputEvent& event) {
    if (engine_.isGameOver()) {
        return false;
    }

    if (event.button == userSettings_.buttonMapping.softDrop) {
        if (event.type == InputEventType::Press) {
            return engine_.applyAction(GameEngine::Action::SoftDropPressed, event.tickMs);
        }

        if (event.type == InputEventType::Release) {
            return engine_.applyAction(GameEngine::Action::SoftDropReleased, event.tickMs);
        }

        return false;
    }

    GameEngine::Action action = GameEngine::Action::MoveLeft;

    if (event.type == InputEventType::Press && mapsToAction(event.button, action)) {
        return engine_.applyAction(action, event.tickMs);
    }

    if (event.type == InputEventType::Repeat && mapsToRepeatAction(event.button, action)) {
        return engine_.applyAction(action, event.tickMs);
    }

    return false;
}

bool GameController::tick(uint32_t nowMs) {
    if (engine_.isGameOver()) {
        return false;
    }

    return engine_.tick(nowMs);
}

bool GameController::isGameOver() const {
    return engine_.isGameOver();
}

void GameController::makeDisplayConfigEvent(RenderEvent& event) const {
    event = {};
    event.type = RenderEventType::Config;
    event.payload.config = {
        userSettings_.theme,
        userSettings_.holdEnabled,
        userSettings_.ghostEnabled,
        userSettings_.nextPreviewCount
    };
}

void GameController::makeGameplayRenderEvent(RenderEvent& event) const {
    event = {};
    event.type = RenderEventType::Screen;
    event.payload.screen.screen = RenderScreen::Gameplay;
    event.payload.screen.payload.gameplay = {
        engine_.renderState()
    };
}

void GameController::makeGameOverRenderEvent(RenderEvent& event) const {
    event = {};
    event.type = RenderEventType::Screen;
    event.payload.screen.screen = RenderScreen::GameOver;
    event.payload.screen.payload.gameOver = {
        engine_.renderState(),
        engine_.gameOverReason()
    };
}

bool GameController::mapsToAction(PhysicalButton button, GameEngine::Action& action) const {
    if (button == userSettings_.buttonMapping.moveLeft) {
        action = GameEngine::Action::MoveLeft;
        return true;
    }

    if (button == userSettings_.buttonMapping.moveRight) {
        action = GameEngine::Action::MoveRight;
        return true;
    }

    if (button == userSettings_.buttonMapping.hardDrop) {
        action = GameEngine::Action::HardDrop;
        return true;
    }

    if (button == userSettings_.buttonMapping.rotateCw) {
        action = GameEngine::Action::RotateCw;
        return true;
    }

    if (button == userSettings_.buttonMapping.rotateCcw) {
        action = GameEngine::Action::RotateCcw;
        return true;
    }

    if (button == userSettings_.buttonMapping.hold) {
        action = GameEngine::Action::Hold;
        return true;
    }

    return false;
}

bool GameController::mapsToRepeatAction(PhysicalButton button, GameEngine::Action& action) const {
    if (button == userSettings_.buttonMapping.moveLeft) {
        action = GameEngine::Action::MoveLeft;
        return true;
    }

    if (button == userSettings_.buttonMapping.moveRight) {
        action = GameEngine::Action::MoveRight;
        return true;
    }

    return false;
}
