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

struct CenterResetState {
    bool wasPressed;
    bool triggered;
    uint32_t pressedSinceMs;
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

void primeButtonStates(ButtonState buttonStates[BUTTON_COUNT], uint32_t nowMs) {
    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        const bool pressed = readPressed(BUTTONS[i].pin);
        buttonStates[i] = {
            pressed,
            pressed,
            nowMs,
            pressed && canPhysicallyRepeat(BUTTONS[i].button)
                ? nowMs + INPUT_HOLD_DETECT_MS
                : 0
        };
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

void pollCenterSoftwareReset(const ButtonState& centerState, CenterResetState& resetState, uint32_t nowMs) {
    if (!centerState.debouncedPressed) {
        resetState.wasPressed = false;
        resetState.triggered = false;
        resetState.pressedSinceMs = 0;
        return;
    }

    if (!resetState.wasPressed) {
        resetState.wasPressed = true;
        resetState.triggered = false;
        resetState.pressedSinceMs = nowMs;
        return;
    }

    if (!resetState.triggered && (nowMs - resetState.pressedSinceMs) >= INPUT_CENTER_RESET_HOLD_MS) {
        resetState.triggered = true;
        vTaskDelay(INPUT_RESET_DELAY_TICKS);
        ESP.restart();
    }
}

}  // namespace

void inputTask(void* pvParameters) {
    (void)pvParameters;

    initButtons();
    vTaskDelay(INPUT_STARTUP_SETTLE_TICKS);

    ButtonState buttonStates[BUTTON_COUNT] = {};
    primeButtonStates(buttonStates, millis());
    CenterResetState centerResetState = {};
    TickType_t lastWakeTick = xTaskGetTickCount();

    while(true) {
        const uint32_t nowMs = millis();

        for (size_t i = 0; i < BUTTON_COUNT; ++i) {
            pollButton(BUTTONS[i], buttonStates[i], nowMs);
        }
        pollCenterSoftwareReset(buttonStates[BUTTON_COUNT - 1], centerResetState, nowMs);

        vTaskDelayUntil(&lastWakeTick, INPUT_POLL_PERIOD_TICKS);
    }
}
