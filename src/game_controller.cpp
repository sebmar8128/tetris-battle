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

    outboundHead_ = 0;
    outboundCount_ = 0;
    localGameOverPacketQueued_ = false;
}

bool GameController::handleInputEvent(const InputEvent& event) {
    if (engine_.isGameOver()) {
        return false;
    }

    if (event.button == userSettings_.buttonMapping.softDrop) {
        if (event.type == InputEventType::Press) {
            return handleStepResult(engine_.applyAction(GameEngine::Action::SoftDropPressed, event.tickMs));
        }

        if (event.type == InputEventType::Release) {
            return handleStepResult(engine_.applyAction(GameEngine::Action::SoftDropReleased, event.tickMs));
        }

        return false;
    }

    GameEngine::Action action = GameEngine::Action::MoveLeft;

    if (event.type == InputEventType::Press && mapsToAction(event.button, action)) {
        return handleStepResult(engine_.applyAction(action, event.tickMs));
    }

    if (event.type == InputEventType::Repeat && mapsToRepeatAction(event.button, action)) {
        return handleStepResult(engine_.applyAction(action, event.tickMs));
    }

    return false;
}

bool GameController::handleRemoteEvent(const RemoteEvent& event) {
    if (event.type != RemoteEventType::PacketReceived) {
        return false;
    }

    switch (event.packet.type) {
        case PacketType::Garbage:
        {
            const GarbagePacket& garbage = event.packet.payload.garbage;
            return handleStepResult(engine_.applyGarbage({
                garbage.lines,
                garbage.holeColumn
            }));
        }
        case PacketType::GameOver:
            return false;
        case PacketType::LobbyState:
        case PacketType::StartGameRequest:
        case PacketType::StartGameReply:
        case PacketType::Pause:
        case PacketType::PauseActionRequest:
        case PacketType::PauseActionReply:
            return false;
    }

    return false;
}

bool GameController::tick(uint32_t nowMs) {
    if (engine_.isGameOver()) {
        return false;
    }

    return handleStepResult(engine_.tick(nowMs));
}

bool GameController::popOutboundPacket(NetPacket& packet) {
    if (outboundCount_ == 0) {
        return false;
    }

    packet = outboundPackets_[outboundHead_];
    outboundHead_ = (outboundHead_ + 1) % OUTBOUND_PACKET_CAPACITY;
    --outboundCount_;
    return true;
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

bool GameController::handleStepResult(const GameEngine::StepResult& result) {
    if (result.hasOutgoingGarbage) {
        queueGarbagePacket(result.outgoingGarbage);
    }

    if (result.gameOver) {
        queueGameOverPacket();
    }

    return result.changed;
}

bool GameController::queueOutboundPacket(const NetPacket& packet) {
    if (outboundCount_ >= OUTBOUND_PACKET_CAPACITY) {
        return false;
    }

    const uint8_t tail = (outboundHead_ + outboundCount_) % OUTBOUND_PACKET_CAPACITY;
    outboundPackets_[tail] = packet;
    ++outboundCount_;
    return true;
}

void GameController::queueGarbagePacket(const GameEngine::GarbageAttack& garbage) {
    NetPacket packet = {};
    packet.type = PacketType::Garbage;
    packet.payload.garbage = {
        garbage.lines,
        garbage.holeColumn
    };
    (void)queueOutboundPacket(packet);
}

void GameController::queueGameOverPacket() {
    if (localGameOverPacketQueued_) {
        return;
    }

    const PlayerRenderState state = engine_.renderState();
    NetPacket packet = {};
    packet.type = PacketType::GameOver;
    packet.payload.gameOver = {
        engine_.gameOverReason(),
        state.score,
        state.linesCleared
    };

    localGameOverPacketQueued_ = queueOutboundPacket(packet);
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
