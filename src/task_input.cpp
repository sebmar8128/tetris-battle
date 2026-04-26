#include <Arduino.h>

#include "config.h"
#include "queues.h"
#include "tasks.h"
#include "types.h"

namespace {

struct ButtonConfig {
    PhysicalButton button;
    int8_t pin;
};

struct ButtonState {
    bool rawPressed;
    bool debouncedPressed;
    uint32_t lastRawChangeMs;
    uint32_t nextRepeatMs;
};

constexpr ButtonConfig BUTTONS[] = {
    {PhysicalButton::LdUp,    PIN_BUTTON_LD_UP},
    {PhysicalButton::LdLeft,  PIN_BUTTON_LD_LEFT},
    {PhysicalButton::LdRight, PIN_BUTTON_LD_RIGHT},
    {PhysicalButton::LdDown,  PIN_BUTTON_LD_DOWN},
    {PhysicalButton::RdUp,    PIN_BUTTON_RD_UP},
    {PhysicalButton::RdLeft,  PIN_BUTTON_RD_LEFT},
    {PhysicalButton::RdRight, PIN_BUTTON_RD_RIGHT},
    {PhysicalButton::RdDown,  PIN_BUTTON_RD_DOWN},
    {PhysicalButton::Center,  PIN_BUTTON_CENTER},
};

constexpr size_t BUTTON_COUNT = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

bool readPressed(int8_t pin) {
    return digitalRead(pin) == LOW;
}

bool canPhysicallyRepeat(PhysicalButton button) {
    return button != PhysicalButton::Center;
}

void emitInputEvent(PhysicalButton button, InputEventType type, uint32_t nowMs) {
    const InputEvent event = {
        button,
        type,
        nowMs
    };

    (void)xQueueSend(g_inputEventQueue, &event, 0);
}

void initButtons() {
    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        pinMode(BUTTONS[i].pin, INPUT_PULLUP);
    }
}

void pollButton(const ButtonConfig& config, ButtonState& state, uint32_t nowMs) {
    const bool rawPressed = readPressed(config.pin);

    if (rawPressed != state.rawPressed) {
        state.rawPressed = rawPressed;
        state.lastRawChangeMs = nowMs;
    }

    const uint32_t stableMs = nowMs - state.lastRawChangeMs;
    if (stableMs >= INPUT_DEBOUNCE_MS && state.debouncedPressed != state.rawPressed) {
        state.debouncedPressed = state.rawPressed;

        if (state.debouncedPressed) {
            state.nextRepeatMs = nowMs + INPUT_HOLD_DETECT_MS;
            emitInputEvent(config.button, InputEventType::Press, nowMs);
        } else {
            state.nextRepeatMs = 0;
            emitInputEvent(config.button, InputEventType::Release, nowMs);
        }
    }

    while (
        canPhysicallyRepeat(config.button) &&
        state.debouncedPressed &&
        state.nextRepeatMs != 0 &&
        static_cast<int32_t>(nowMs - state.nextRepeatMs) >= 0
    ) {
        emitInputEvent(config.button, InputEventType::Repeat, nowMs);
        state.nextRepeatMs += INPUT_REPEAT_PERIOD_MS;
    }
}

}  // namespace

void inputTask(void* pvParameters) {
    (void)pvParameters;

    initButtons();

    ButtonState buttonStates[BUTTON_COUNT] = {};
    TickType_t lastWakeTick = xTaskGetTickCount();

    while(true) {
        const uint32_t nowMs = millis();

        for (size_t i = 0; i < BUTTON_COUNT; ++i) {
            pollButton(BUTTONS[i], buttonStates[i], nowMs);
        }

        vTaskDelayUntil(&lastWakeTick, INPUT_POLL_PERIOD_TICKS);
    }
}
