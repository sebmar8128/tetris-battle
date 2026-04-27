#pragma once

#include <Arduino.h>
#include <string.h>

#include "types.h"

namespace DefaultSettings {

static constexpr uint32_t GAME_SEED = 0x41802026;

inline UserSettings userSettings() {
    UserSettings settings = {};

    strncpy(settings.username, "PLAYER", MAX_USERNAME_LEN);
    settings.username[MAX_USERNAME_LEN] = '\0';

    settings.buttonMapping = {
        PhysicalButton::LdLeft,
        PhysicalButton::LdRight,
        PhysicalButton::LdDown,
        PhysicalButton::LdUp,
        PhysicalButton::RdUp,
        PhysicalButton::RdLeft,
        PhysicalButton::RdDown
    };
    settings.theme = ThemeId::Modern;
    settings.holdEnabled = true;
    settings.ghostEnabled = true;
    settings.nextPreviewCount = MAX_NEXT_PIECES;

    return settings;
}

inline MatchSettings matchSettings() {
    MatchSettings settings = {};
    settings.garbageEnabled = true;
    settings.mode = GameMode::Marathon;
    settings.startingLevel = 0;
    settings.musicEnabled = true;
    return settings;
}

}  // namespace DefaultSettings
