#pragma once

#include <Arduino.h>

enum class MusicEventType : uint8_t {
    Start,
    Stop,
    Pause,
    Resume
};

struct MusicEvent {
    MusicEventType type;
};

enum class NoteName : uint16_t {
    Rest = 0,
    Gs4 = 415,
    A4  = 440,
    B4  = 494,
    C5  = 523,
    D5  = 587,
    E5  = 659,
    F5  = 698,
    G5  = 784,
    A5  = 880
};

enum class NoteLength : uint8_t {
    Sixteenth      = 1,
    Eighth         = 2,
    DottedEighth   = 3,
    Quarter        = 4,
    DottedQuarter  = 6,
    Half           = 8,
    Whole          = 16
};

struct MusicNote {
    NoteName note;
    NoteLength length;
};

struct MusicTrackDef {
    const MusicNote* notes;
    uint16_t noteCount;
    uint16_t bpm;
};

const MusicTrackDef& korobeinikiTrack();
