#include "music.h"

namespace {

using L = NoteLength;
using N = NoteName;

constexpr MusicNote KOROBEINIKI_NOTES[] = {
    {N::E5, L::Quarter}, {N::B4, L::Eighth}, {N::C5, L::Eighth},
    {N::D5, L::Quarter}, {N::C5, L::Eighth}, {N::B4, L::Eighth},
    {N::A4, L::Quarter}, {N::A4, L::Eighth}, {N::C5, L::Eighth},
    {N::E5, L::Quarter}, {N::D5, L::Eighth}, {N::C5, L::Eighth},

    {N::B4, L::DottedQuarter}, {N::C5, L::Eighth},
    {N::D5, L::Quarter}, {N::E5, L::Quarter},
    {N::C5, L::Quarter}, {N::A4, L::Quarter},
    {N::A4, L::Half},

    {N::Rest, L::Eighth}, {N::D5, L::Quarter}, {N::F5, L::Eighth}, {N::A5, L::Quarter},
    {N::G5, L::Eighth}, {N::F5, L::Eighth}, {N::E5, L::DottedQuarter},
    {N::C5, L::Eighth}, {N::E5, L::Quarter},
    {N::D5, L::Eighth}, {N::C5, L::Eighth},

    {N::B4, L::Quarter}, {N::B4, L::Eighth}, {N::C5, L::Eighth}, {N::D5, L::Quarter},
    {N::E5, L::Quarter}, {N::C5, L::Quarter},
    {N::A4, L::Quarter}, {N::A4, L::Quarter},
    {N::Rest, L::Quarter},

    {N::E5, L::Half}, {N::C5, L::Half},
    {N::D5, L::Half}, {N::B4, L::Half},
    {N::C5, L::Half}, {N::A4, L::Half},
    {N::Gs4, L::Half}, {N::B4, L::Half},

    {N::E5, L::Half}, {N::C5, L::Half},
    {N::D5, L::Half}, {N::B4, L::Half},
    {N::C5, L::Quarter}, {N::E5, L::Quarter},
    {N::A5, L::Half}, {N::Gs4, L::Half},
};

constexpr MusicTrackDef KOROBEINIKI_TRACK = {
    KOROBEINIKI_NOTES,
    static_cast<uint16_t>(sizeof(KOROBEINIKI_NOTES) / sizeof(KOROBEINIKI_NOTES[0])),
    170
};

}  // namespace

const MusicTrackDef& korobeinikiTrack() {
    return KOROBEINIKI_TRACK;
}
