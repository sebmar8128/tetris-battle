#pragma once

#include <Arduino.h>

#include "events.h"

namespace Display {

bool begin();
void configure(const DisplayRenderConfig& config);
void renderScreen(const ScreenRenderState& screen);

}  // namespace Display
