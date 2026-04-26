#pragma once

#include <Arduino.h>

#include "events.h"
#include "game_engine.h"

class GameController {
public:
    void begin();
    bool handleInputEvent(const InputEvent& event);
    bool tick(uint32_t nowMs);

    bool isGameOver() const;

    void makeDisplayConfigEvent(RenderEvent& event) const;
    void makeGameplayRenderEvent(RenderEvent& event) const;
    void makeGameOverRenderEvent(RenderEvent& event) const;

private:
    bool mapsToAction(PhysicalButton button, GameEngine::Action& action) const;
    bool mapsToRepeatAction(PhysicalButton button, GameEngine::Action& action) const;

    UserSettings userSettings_;
    MatchSettings matchSettings_;
    GameEngine::Engine engine_;
};
