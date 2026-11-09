//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/InsertSlotListComponent.h"

/**
 * The mixer's measurements. Every element of a strip has a fixed height, so
 * the mixer's height follows from what is shown (the sections the user has
 * switched on in the section bar, and the number of send rows) rather than
 * from a drag: the seam above the mixer only closes it.
 */
namespace MixerLayout
{
    constexpr int stripWidth = 60;
    constexpr int stripGap = 1;
    constexpr int groupGap = 6;
    constexpr int bandHeight = 18;

    constexpr int insertRows = 5;              ///< slot rows of a track's insert list (it scrolls when the chain is longer)
    constexpr int ioRowHeight = 12;
    constexpr int knobNameHeight = 10;         ///< the label over a knob: the aux name, "Pan"
    constexpr int knobHeight = 30;             ///< a knob's dial, pan and sends alike
    constexpr int panHeight = knobNameHeight + knobHeight;
    constexpr int buttonRowHeight = 15;
    constexpr int buttonWidth = 15;
    constexpr int buttonGap = 5;
    constexpr int faderX = -3;                 ///< the fader's left edge in the strip; the slider centres its slot in its width
    constexpr int faderY = 0;                  ///< the fader's top in the fader element
    constexpr int faderWidth = 42;
    constexpr int faderHeight = 160;           ///< the fader's travel area
    constexpr int nameHeight = 16;
    constexpr int colourBarHeight = 3;
    constexpr int meterX = 25;                 ///< the meter's left edge in the strip, its dB scale first and the bar right of it
    constexpr int meterY = 0;                  ///< the meter's top in the fader element; its bar starts getBarInset() below that
    constexpr int meterHeight = 156;           ///< the bar lines up with the fader's travel: the slider indents its slot by the thumb radius (12)
    constexpr int meterWidth = 7;
    constexpr int stereoBarWidth = 6;           ///< a return strip's meter bars, left and right; narrower than a channel's one bar
    constexpr int sidePadding = 2;
    constexpr int sectionGap = 3;
    constexpr int scrollBarHeight = 8;         ///< the mixer's horizontal scroll bar, always given its room

    /// The strip sections a user can hide, as flags; the fader, meter and name always show.
    enum Section : int
    {
        inserts = 1 << 0,
        sends   = 1 << 1,
        io      = 1 << 2,
        pan     = 1 << 3,
        buttons = 1 << 4,
    };
    constexpr int allSections = inserts | sends | io | pan | buttons;

    constexpr int sendKnobsPerRow = 1;

    /// Rows of send knobs for a number of aux buses, one knob per aux, above each other; one row even without any.
    inline int sendRows (int numAuxBuses) noexcept
    {
        return (juce::jmax (1, numAuxBuses) + sendKnobsPerRow - 1) / sendKnobsPerRow;
    }

    /// The send rows' height: a row is a knob with its name, with a section gap between the rows as between
    /// the sends and the next section.
    inline int sendsHeight (int numAuxBuses) noexcept
    {
        const auto rows = sendRows (numAuxBuses);
        return rows * (knobNameHeight + knobHeight) + (rows - 1) * sectionGap;
    }

    /// A strip's height with these sections shown (the I/O rows are not the strip's: see ioHeight).
    inline int stripHeight (int sections, int numAuxBuses) noexcept
    {
        int height = sectionGap;
        if ((sections & sends) != 0)
            height += sendsHeight (numAuxBuses) + sectionGap;
        if ((sections & pan) != 0)
            height += panHeight;
        if ((sections & buttons) != 0)
            height += buttonGap + 2 * (buttonRowHeight + buttonGap);
        return height + faderY + faderHeight + nameHeight + colourBarHeight;
    }

    /// The I/O rows under the band, one per channel in its strip's column, with a gap above: shown by the group
    /// above the track's insert list, so the strips below start with their sends.
    inline int ioHeight (int sections) noexcept
    {
        return (sections & io) != 0 ? sectionGap + 2 * ioRowHeight : 0;
    }

    /// A track group's height: the band, the I/O rows, the insert list and the strips.
    inline int groupHeight (int sections, int numAuxBuses) noexcept
    {
        const auto insertList = (sections & inserts) != 0 ? 2 + InsertSlotListComponent::heightForRows (insertRows) : 0;
        return bandHeight + ioHeight (sections) + insertList + stripHeight (sections, numAuxBuses);
    }

    /// The mixer's height: a group and the scroll bar under it.
    inline int mixerHeight (int sections, int numAuxBuses) noexcept
    {
        return groupHeight (sections, numAuxBuses) + scrollBarHeight;
    }

    /// The fader's travel area in a group's y, faderHeight tall: above the name bar at the group's bottom,
    /// under the fader element's faderY of headroom. The section bar lines its toggles up with it.
    inline juce::Range<int> faderRange (int sections, int numAuxBuses) noexcept
    {
        const auto bottom = groupHeight (sections, numAuxBuses) - nameHeight - colourBarHeight;
        return { bottom - faderHeight, bottom };
    }
}
