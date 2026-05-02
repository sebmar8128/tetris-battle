#include <Arduino.h>

#include "config.h"
#include "music.h"
#include "queues.h"

namespace {

constexpr uint8_t BUZZER_LEDC_CHANNEL = 0;
constexpr uint8_t BUZZER_LEDC_RESOLUTION_BITS = 10;
constexpr uint16_t BUZZER_LEDC_MAX_DUTY = (1U << BUZZER_LEDC_RESOLUTION_BITS) - 1U;
constexpr uint16_t BUZZER_LEDC_DUTY = (BUZZER_LEDC_MAX_DUTY * 2U) / 100U;
constexpr uint32_t BUZZER_INITIAL_FREQ_HZ = 440;
constexpr TickType_t MUSIC_COMMAND_POLL_TICKS = pdMS_TO_TICKS(5);
constexpr TickType_t MUSIC_REPEAT_DELAY_TICKS = pdMS_TO_TICKS(1000);

uint32_t noteDurationMs(const MusicTrackDef& track, NoteLength length) {
    const uint32_t sixteenthUnits = static_cast<uint8_t>(length);
    return (60000UL * sixteenthUnits) / (static_cast<uint32_t>(track.bpm) * 4UL);
}

void buzzerOff() {
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
}

void playNote(NoteName note) {
    const uint32_t freq = static_cast<uint16_t>(note);
    if (freq == 0) {
        buzzerOff();
        return;
    }

    (void)ledcWriteTone(BUZZER_LEDC_CHANNEL, freq);
    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_LEDC_DUTY);
}

void configureBuzzer() {
    (void)ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_INITIAL_FREQ_HZ, BUZZER_LEDC_RESOLUTION_BITS);
    ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
    buzzerOff();
}

void handleMusicEvent(const MusicEvent& event, bool& playing, bool& paused, uint16_t& noteIndex) {
    switch (event.type) {
        case MusicEventType::Start:
            noteIndex = 0;
            paused = false;
            playing = true;
            return;
        case MusicEventType::Stop:
            playing = false;
            paused = false;
            noteIndex = 0;
            buzzerOff();
            return;
        case MusicEventType::Pause:
            paused = true;
            buzzerOff();
            return;
        case MusicEventType::Resume:
            if (playing) {
                paused = false;
            }
            return;
    }
}

bool waitForDurationOrCommand(TickType_t durationTicks, bool& playing, bool& paused, uint16_t& noteIndex) {
    TickType_t remaining = durationTicks;

    while (remaining > 0) {
        const TickType_t waitTicks = remaining < MUSIC_COMMAND_POLL_TICKS
            ? remaining
            : MUSIC_COMMAND_POLL_TICKS;

        MusicEvent event = {};
        if (xQueueReceive(g_musicEventQueue, &event, waitTicks) == pdTRUE) {
            handleMusicEvent(event, playing, paused, noteIndex);
            if (!playing || paused || noteIndex == 0) {
                return false;
            }
        }

        if (remaining <= waitTicks) {
            return true;
        }
        remaining -= waitTicks;
    }

    return true;
}

void waitForRepeatDelay(bool& playing, bool& paused, uint16_t& noteIndex) {
    buzzerOff();
    (void)waitForDurationOrCommand(MUSIC_REPEAT_DELAY_TICKS, playing, paused, noteIndex);
}

}  // namespace

void musicTask(void* pvParameters) {
    (void)pvParameters;

    configureBuzzer();

    const MusicTrackDef& track = korobeinikiTrack();
    bool playing = false;
    bool paused = false;
    uint16_t noteIndex = 0;

    while (true) {
        if (!playing || paused) {
            buzzerOff();
            MusicEvent event = {};
            if (xQueueReceive(g_musicEventQueue, &event, portMAX_DELAY) == pdTRUE) {
                handleMusicEvent(event, playing, paused, noteIndex);
            }
            continue;
        }

        if (noteIndex >= track.noteCount) {
            noteIndex = 0;
            waitForRepeatDelay(playing, paused, noteIndex);
            continue;
        }

        const MusicNote& note = track.notes[noteIndex];
        playNote(note.note);

        const TickType_t durationTicks = pdMS_TO_TICKS(noteDurationMs(track, note.length));
        if (waitForDurationOrCommand(durationTicks, playing, paused, noteIndex)) {
            ++noteIndex;
        }
    }
}
