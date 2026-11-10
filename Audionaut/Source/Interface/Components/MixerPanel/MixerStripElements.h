//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Interface/Components/MixerPanel/MixerLayout.h"
#include "Interface/Components/MixerPanel/SendKnobComponent.h"
#include "Interface/Controls/LevelMeter.h"
#include "Engine/Group/AudioTrack.h"

/**
 * The elements a mixer strip is stacked from, one component each: sends,
 * pan, the record / monitor / solo / mute buttons, the fader with its
 * meter, and the name bar; and the channel's I/O rows, which the track
 * group lays out above the strips. Each works on one bus channel
 * (StripChannel), re-reads it on refresh(), and sends its edits to the
 * engine the way the arrangement's channel strip does (the track's
 * drag-start / drag-end undo bracket). Heights come from MixerLayout, so
 * MixerLayout::stripHeight() and the strip's layout agree.
 */

/// What every element works on: the mixer's context and the bus channel it shows.
struct StripChannel
{
    MixerContext context;
    std::shared_ptr<audium::AudioTrack> track;
    int channelNumber = 0;

    std::shared_ptr<audium::AudioChannel> channel() const { return track->getChannel (channelNumber); }
    audium::AudioChannelData data() const { return track->getChannelData (channelNumber); }
};

//==============================================================================
/// The sends: one knob per aux bus, above each other in the aux buses' order.
class StripSends : public juce::Component
{
public:
    explicit StripSends (StripChannel channel);

    static int heightFor (int numAuxBuses) { return MixerLayout::sendsHeight (numAuxBuses); }

    void refresh();
    void resized() override;

private:
    StripChannel channel;
    std::vector<std::unique_ptr<SendKnobComponent>> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StripSends)
};

//==============================================================================
/// Input over output: the route's name, dim when the device lacks it by default, red when a chosen route is missing; click opens the routing menu.
class StripIo : public juce::Component
{
public:
    explicit StripIo (StripChannel channel);
    ~StripIo() override;

    static constexpr int height = 2 * MixerLayout::ioRowHeight;

    void refresh();
    void resized() override;

private:
    class RouteButton;

    StripChannel channel;
    std::unique_ptr<RouteButton> input, output;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StripIo)
};

//==============================================================================
/// "Pan" over the dial, labelled like the send knobs; disabled while a direct out bypasses the pan.
class StripPan : public juce::Component
{
public:
    explicit StripPan (StripChannel channel);

    static constexpr int height = MixerLayout::panHeight;

    void refresh();
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    StripChannel channel;
    juce::Slider slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StripPan)
};

//==============================================================================
/// Two rows of two, as on a Pro Tools strip: record and input monitor, then solo and mute.
class StripButtons : public juce::Component
{
public:
    explicit StripButtons (StripChannel channel);

    static constexpr int height = MixerLayout::buttonGap + 2 * (MixerLayout::buttonRowHeight + MixerLayout::buttonGap);

    void refresh();
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    StripChannel channel;
    juce::DrawableButton record { "Record", juce::DrawableButton::ImageOnButtonBackground };
    juce::TextButton monitor { "I" }, solo { "S" }, mute { "M" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StripButtons)
};

//==============================================================================
/// The fader on the console taper and the level meter with its scale beside it.
class StripFader : public juce::Component
{
public:
    explicit StripFader (StripChannel channel);

    static constexpr int height = MixerLayout::faderY + MixerLayout::faderHeight;

    void refresh();
    void refreshLive();   ///< the meter, on the mixer's timer
    void resized() override;

private:
    StripChannel channel;
    int busChannel = 0;
    juce::Slider fader;
    LevelMeter meter { true, false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StripFader)
};

//==============================================================================
/// The channel's name on a tint of the track's colour, over the colour bar; click selects the channel.
class StripName : public juce::Component
{
public:
    explicit StripName (StripChannel channel);

    static constexpr int height = MixerLayout::nameHeight + MixerLayout::colourBarHeight;

    void refresh();
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    StripChannel channel;
    juce::String name;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StripName)
};
