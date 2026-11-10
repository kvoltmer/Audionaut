//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <array>
#include <bit>

#include "Interface/Components/MixerPanel/InsertSlotListComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

/**
 * The mixer's measurements. Every element of a strip has a fixed height, so
 * the mixer's height follows from what is shown (the sections the user has
 * switched on in the section bar, and the number of send rows) rather than
 * from a drag: dragging the header bar only closes and opens the mixer.
 * A section's room is its full height scaled by how far the section is open
 * (Shown): the mixer animates a toggle by moving that amount, and the strips,
 * the groups and the mixer's height all follow it frame by frame.
 */
namespace MixerLayout
{
    constexpr int headerHeight = AudiumLookAndFeel::dividerHeight;   ///< the band across the mixer's top: the seam and the handle
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
    constexpr int buttonsHeight = buttonGap + 2 * (buttonRowHeight + buttonGap);   ///< the two button rows with their gaps
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
    constexpr int numSections = 5;

    /// How far each section is open: 1 shown, 0 hidden, in between while the mixer animates a toggle. The
    /// layout gives a section that much of its full height, so every measurement below moves with the animation.
    struct Shown
    {
        std::array<float, numSections> amounts {};   ///< by the section's bit index

        /// Open or closed, no animation: the sections in the mask open, the rest closed.
        static Shown of (int sections) noexcept
        {
            Shown shown;
            for (int i = 0; i < numSections; ++i)
                shown.amounts[static_cast<size_t> (i)] = (sections & (1 << i)) != 0 ? 1.0f : 0.0f;
            return shown;
        }

        float of (Section section) const noexcept { return amounts[indexOf (section)]; }
        float& of (Section section) noexcept { return amounts[indexOf (section)]; }

        bool operator== (const Shown&) const noexcept = default;

    private:
        static size_t indexOf (Section section) noexcept { return static_cast<size_t> (std::countr_zero (static_cast<unsigned> (section))); }
    };

    /// As much of a full height as a section is open.
    inline int heightShown (int fullHeight, float amount) noexcept
    {
        return juce::roundToInt (fullHeight * amount);
    }

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

    /// The sends' room when open: the rows and the gap to the next section.
    inline int sendsRoomHeight (int numAuxBuses) noexcept
    {
        return sendsHeight (numAuxBuses) + sectionGap;
    }

    /// A strip's height with the sections this far open (the I/O rows are not the strip's: see ioHeight).
    inline int stripHeight (const Shown& shown, int numAuxBuses) noexcept
    {
        return sectionGap
             + heightShown (sendsRoomHeight (numAuxBuses), shown.of (sends))
             + heightShown (panHeight, shown.of (pan))
             + heightShown (buttonsHeight, shown.of (buttons))
             + faderY + faderHeight + nameHeight + colourBarHeight;
    }

    /// Where a strip's elements stand in the strip's bounds, each as tall as stripHeight() counts it: the name
    /// bar at the bottom, then from the top the sends, the pan, the buttons and the fader. A hidden section's
    /// rectangle is empty; one on its way open or closed has as much height as the animation has reached, for
    /// the strip to clip its element to (SectionRoom). The channel strips and the return strips both lay out
    /// from this, so their faders line up across the mixer.
    struct StripPlacement
    {
        juce::Rectangle<int> sendsArea, panArea, buttonsArea, faderArea, nameArea;
    };

    inline StripPlacement placeStrip (juce::Rectangle<int> bounds, const Shown& shown, int numAuxBuses) noexcept
    {
        StripPlacement placement;
        placement.nameArea = bounds.removeFromBottom (nameHeight + colourBarHeight);
        bounds.reduce (sidePadding, 0);
        bounds.removeFromTop (sectionGap);
        placement.sendsArea = bounds.removeFromTop (heightShown (sendsRoomHeight (numAuxBuses), shown.of (sends)));
        placement.sendsArea.removeFromBottom (juce::jmin (sectionGap, placement.sendsArea.getHeight()));   // the gap to the next section, as far as the room has it
        placement.panArea = bounds.removeFromTop (heightShown (panHeight, shown.of (pan)));
        placement.buttonsArea = bounds.removeFromTop (heightShown (buttonsHeight, shown.of (buttons)));
        placement.faderArea = bounds.removeFromTop (faderY + faderHeight);
        return placement;
    }

    /// The I/O rows' room when open: a gap under the band, then one row per channel in its strip's column.
    /// The group shows them above the track's insert list, so the strips below start with their sends.
    constexpr int ioRoomHeight = sectionGap + 2 * ioRowHeight;

    inline int ioHeight (const Shown& shown) noexcept
    {
        return heightShown (ioRoomHeight, shown.of (io));
    }

    /// The insert list's room when open: the list with a line's gap above and below.
    inline int insertsRoomHeight() noexcept
    {
        return 2 + InsertSlotListComponent::heightForRows (insertRows);
    }

    inline int insertsHeight (const Shown& shown) noexcept
    {
        return heightShown (insertsRoomHeight(), shown.of (inserts));
    }

    /// A track group's height: the band, the I/O rows, the insert list and the strips.
    inline int groupHeight (const Shown& shown, int numAuxBuses) noexcept
    {
        return bandHeight + ioHeight (shown) + insertsHeight (shown) + stripHeight (shown, numAuxBuses);
    }

    /// The mixer's height: the header bar, a group and the scroll bar under it.
    inline int mixerHeight (const Shown& shown, int numAuxBuses) noexcept
    {
        return headerHeight + groupHeight (shown, numAuxBuses) + scrollBarHeight;
    }

    /// The fader's travel area in a group's y, faderHeight tall: above the name bar at the group's bottom,
    /// under the fader element's faderY of headroom. The section bar lines its toggles up with it.
    inline juce::Range<int> faderRange (const Shown& shown, int numAuxBuses) noexcept
    {
        const auto bottom = groupHeight (shown, numAuxBuses) - nameHeight - colourBarHeight;
        return { bottom - faderHeight, bottom };
    }

    /**
     * The room a strip gives one section: the section's elements laid out at
     * their full height inside, anchored at the top. While the mixer animates
     * the section open or closed the room grows or shrinks over them, so they
     * are clipped rather than squashed, and they fade with the room. setRoom()
     * takes the area the layout gives the section (empty while hidden) and
     * the elements' full height.
     */
    class SectionRoom : public juce::Component
    {
    public:
        SectionRoom() { setInterceptsMouseClicks (false, true); }

        /// Lays the elements out in the room's full height; the room shows as much of them as it has.
        std::function<void (juce::Rectangle<int> full)> layOut;

        /// Paints under the elements across the room's full height: a caption, the buttons' shadows.
        std::function<void (juce::Graphics& g, juce::Rectangle<int> full)> paintBehind;

        /// Holds one element, laid out across the room's full height.
        void hold (juce::Component& element)
        {
            addAndMakeVisible (element);
            layOut = [&element] (juce::Rectangle<int> full) { element.setBounds (full); };
        }

        void setRoom (juce::Rectangle<int> area, int fullHeight_)
        {
            const auto fullHeightChanged = fullHeight != fullHeight_;
            fullHeight = fullHeight_;
            setVisible (! area.isEmpty());
            setAlpha (fullHeight > 0 ? juce::jlimit (0.0f, 1.0f, static_cast<float> (area.getHeight()) / static_cast<float> (fullHeight)) : 1.0f);
            setBounds (area);
            if (fullHeightChanged)
                resized();
        }

        void resized() override
        {
            if (layOut != nullptr)
                layOut (getLocalBounds().withHeight (fullHeight));
        }

        void paint (juce::Graphics& g) override
        {
            if (paintBehind != nullptr)
                paintBehind (g, getLocalBounds().withHeight (fullHeight));
        }

    private:
        int fullHeight = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionRoom)
    };
}
