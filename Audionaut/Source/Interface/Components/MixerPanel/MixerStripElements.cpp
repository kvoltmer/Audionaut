//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerStripElements.h"

#include "Interface/ColourIds.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"
#include "Engine/Playback/AudioBusInterface.h"

using namespace audium;
using namespace MixerLayout;

//==============================================================================
StripSends::StripSends (StripChannel channel_) : channel (std::move (channel_)) {}

void StripSends::refresh()
{
    const auto& buses = channel.context.container().getAuxBuses().getBuses();

    // the knobs follow the aux buses: rebuilt when the set changed, renamed otherwise
    auto same = knobs.size() == buses.size();
    for (size_t i = 0; same && i < knobs.size(); ++i)
        same = knobs[i]->getAuxId() == buses[i]->getId();
    if (! same) {
        knobs.clear();
        for (const auto& bus : buses) {
            knobs.push_back (std::make_unique<SendKnobComponent> (channel.context, channel.track->getId(), channel.channelNumber,
                                                                  bus->getId(), juce::String (bus->getName())));
            addAndMakeVisible (knobs.back().get());
        }
        resized();
    }

    for (size_t i = 0; i < knobs.size(); ++i) {
        knobs[i]->setAuxName (juce::String (buses[i]->getName()));
        knobs[i]->refresh();
    }
}

void StripSends::resized()
{
    auto bounds = getLocalBounds();
    const auto columnWidth = bounds.getWidth() / sendKnobsPerRow;
    for (size_t i = 0; i < knobs.size(); ++i) {
        const auto row = static_cast<int> (i) / sendKnobsPerRow;
        const auto column = static_cast<int> (i) % sendKnobsPerRow;
        knobs[i]->setBounds (bounds.getX() + column * columnWidth, bounds.getY() + row * (SendKnobComponent::height + sectionGap), columnWidth, SendKnobComponent::height);
    }
}

//==============================================================================
/// One route: its name, red when the device cannot provide it; click opens the routing menu.
class StripIo::RouteButton : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    explicit RouteButton (std::function<void()> onClick_) : onClick (std::move (onClick_))
    {
        setRepaintsOnMouseActivity (true);
    }

    /// How the route reads: there, not on this device by default (dim), or chosen and missing (red).
    enum class State { available, missing, unavailable };

    void set (const juce::String& newText, State newState, const juce::String& tooltip)
    {
        text = newText;
        state = newState;
        setTooltip (tooltip);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (findColour (audium::backgroundColourId).darker (0.3f).withAlpha (isMouseOver() ? 0.8f : 1.0f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 2.0f);
        g.setColour (state == State::unavailable ? juce::Colours::red
                                                 : findColour (audium::defaultTextColourId).withAlpha (state == State::missing ? 0.4f : 0.85f));
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (text, getLocalBounds().reduced (3, 0), juce::Justification::centred, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            juce::NullCheckedInvocation::invoke (onClick);
    }

private:
    std::function<void()> onClick;
    juce::String text;
    State state = State::available;
};

StripIo::StripIo (StripChannel channel_) : channel (std::move (channel_))
{
    // the channel view's routing menus
    input = std::make_unique<RouteButton> ([this]
    {
        ChannelComponent::routingMenu (channel.context.engine, channel.track, channel.channelNumber, true).showMenuAsync (MixerContext::menuOptions (input.get()));
    });
    addAndMakeVisible (input.get());
    output = std::make_unique<RouteButton> ([this]
    {
        ChannelComponent::routingMenu (channel.context.engine, channel.track, channel.channelNumber, false).showMenuAsync (MixerContext::menuOptions (output.get()));
    });
    addAndMakeVisible (output.get());
}

StripIo::~StripIo() = default;

void StripIo::refresh()
{
    const auto data = channel.data();
    juce::StringArray inputNames, outputNames;
    if (auto* device = channel.context.engine->getAudioDeviceManager()->getCurrentAudioDevice()) {
        inputNames = device->getInputChannelNames();
        outputNames = device->getOutputChannelNames();
    }

    // the device's own channel name when it has one, a bare number otherwise
    const auto nameOf = [] (const juce::StringArray& names, int index, const juce::String& prefix)
    {
        const auto name = index < names.size() ? names[index].trim() : juce::String();
        return name.isNotEmpty() ? name : prefix + juce::String (index + 1);
    };

    const auto stateOf = [] (int route, bool chosen, int available)
    {
        if (route < available)
            return RouteButton::State::available;
        return chosen ? RouteButton::State::unavailable : RouteButton::State::missing;
    };

    const auto in = AudioChannelData::effectiveInputChannel (data);
    input->set (nameOf (inputNames, in, TRANS ("in ")), stateOf (in, data.inputChannel >= 0, inputNames.size()),
                data.inputChannel < 0 ? TRANS ("Input (default)") : TRANS ("Input"));

    if (data.outputChannel < 0)
        output->set (TRANS ("Main"), RouteButton::State::available, TRANS ("Output: the main bus"));
    else
        output->set (nameOf (outputNames, data.outputChannel, TRANS ("out ")), stateOf (data.outputChannel, true, outputNames.size()), TRANS ("Output"));
}

void StripIo::resized()
{
    auto bounds = getLocalBounds();
    input->setBounds (bounds.removeFromTop (ioRowHeight));
    output->setBounds (bounds.removeFromTop (ioRowHeight));
}

//==============================================================================
StripPan::StripPan (StripChannel channel_) : channel (std::move (channel_))
{
    // a dial with a pointer and a centre mark, configured like a send knob
    ChannelComponent::configurePanSlider (&slider);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (AudiumLookAndFeel::dialStartAngle, AudiumLookAndFeel::dialEndAngle, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (120);
    slider.onDragStart = [this] { channel.track->onDragStart (channel.channelNumber); };
    slider.onValueChange = [this] { channel.track->setPan (static_cast<float> (slider.getValue()), channel.channelNumber); };
    slider.onDragEnd = [this] { channel.track->onDragEnd ("Set Pan"); };
    addAndMakeVisible (slider);
}

void StripPan::refresh()
{
    const auto data = channel.data();
    slider.setValue (data.pan, juce::dontSendNotification);
    slider.setEnabled (data.outputChannel < 0);   // a direct out bypasses the pan
}

void StripPan::paint (juce::Graphics& g)
{
    g.setColour (findColour (audium::defaultTextColourId).withAlpha (0.85f));
    g.setFont (juce::FontOptions (8.0f));
    g.drawText (TRANS ("Pan"), getLocalBounds().removeFromTop (knobNameHeight), juce::Justification::centred, false);
}

void StripPan::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (knobNameHeight));
}

//==============================================================================
namespace
{
    void styleSmallButton (juce::TextButton& button, juce::Colour onColour)
    {
        button.setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
        button.setColour (juce::TextButton::buttonOnColourId, onColour);
        button.setClickingTogglesState (true);
    }
}

StripButtons::StripButtons (StripChannel channel_) : channel (std::move (channel_))
{
    {
        juce::Path dot;
        dot.addEllipse (0, 0, 5, 5);
        juce::DrawablePath image;
        image.setPath (dot);
        image.setFill (juce::FillType (juce::Colours::red.darker().darker()));
        record.setImages (&image);
    }
    record.setClickingTogglesState (true);
    record.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red.brighter().withAlpha (0.8f));
    record.setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
    record.onClick = [this]
    {
        ChannelComponent::setRecordEnabled (channel.context.engine, channel.track, channel.channelNumber, record.getToggleState());   // tells the strips to re-read
    };
    addAndMakeVisible (record);

    styleSmallButton (monitor, findColour (audium::soloColourId));
    monitor.onClick = [this]
    {
        channel.track->onDragStart (channel.channelNumber);
        auto data = channel.data();
        data.monitor = monitor.getToggleState();
        channel.track->setChannelData (channel.channelNumber, data);
        channel.track->onDragEnd ("Monitor");
    };
    addAndMakeVisible (monitor);

    styleSmallButton (solo, findColour (audium::soloColourId));
    solo.onClick = [this]
    {
        channel.track->onDragStart (channel.channelNumber);
        channel.track->setSolo (solo.getToggleState(), channel.channelNumber);
        channel.track->onDragEnd ("Solo");
    };
    addAndMakeVisible (solo);

    styleSmallButton (mute, findColour (audium::muteColourId));
    mute.onClick = [this]
    {
        channel.track->onDragStart (channel.channelNumber);
        channel.track->setMute (mute.getToggleState(), channel.channelNumber);
        channel.track->onDragEnd ("Mute");
    };
    addAndMakeVisible (mute);

    // console keys: the shadow under a raised one is painted here, so a change of state repaints the whole element
    for (auto* button : std::initializer_list<juce::Button*> { &record, &monitor, &solo, &mute }) {
        button->getProperties().set (AudiumLookAndFeel::consoleButtonProperty, true);
        button->onStateChange = [this] { repaint(); };
    }
}

void StripButtons::refresh()
{
    const auto data = channel.data();
    const auto anySolo = channel.context.container().anyChannelSolo();
    mute.setEnabled (! anySolo);
    mute.setToggleState (data.mute || (anySolo && ! data.solo), juce::dontSendNotification);
    solo.setToggleState (data.solo, juce::dontSendNotification);
    monitor.setToggleState (data.monitor, juce::dontSendNotification);
    record.setToggleState (data.record, juce::dontSendNotification);
    repaint();   // the shadows follow the toggle states
}

void StripButtons::paint (juce::Graphics& g)
{
    AudiumLookAndFeel::drawConsoleButtonShadows (g, { &record, &monitor, &solo, &mute });
}

void StripButtons::resized()
{
    auto bounds = getLocalBounds();
    const auto placePair = [&bounds] (juce::Component& left, juce::Component& right)
    {
        auto row = bounds.removeFromTop (buttonRowHeight).withSizeKeepingCentre (2 * buttonWidth + buttonGap, buttonRowHeight);
        left.setBounds (row.removeFromLeft (buttonWidth));
        row.removeFromLeft (buttonGap);
        right.setBounds (row);
        bounds.removeFromTop (buttonGap);
    };
    bounds.removeFromTop (buttonGap);
    placePair (record, monitor);
    placePair (solo, mute);
}

//==============================================================================
StripFader::StripFader (StripChannel channel_) : channel (std::move (channel_))
{
    AudiumLookAndFeel::configureFaderSlider (&fader);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.onDragStart = [this] { channel.track->onDragStart (channel.channelNumber); };
    fader.onValueChange = [this] { channel.track->setGain (juce::Decibels::decibelsToGain (static_cast<float> (fader.getValue())), channel.channelNumber); };
    fader.onDragEnd = [this] { channel.track->onDragEnd ("Set Gain"); };
    addAndMakeVisible (fader);

    meter.setScaleShown (true);
    meter.setInterceptsMouseClicks (false, false);   // it may overlap the fader's cap
    addAndMakeVisible (meter);
}

void StripFader::refresh()
{
    auto ch = channel.channel();
    if (ch == nullptr)
        return;
    busChannel = ch->getChannelNumber() + channel.track->getChannelOffset();

    if (! fader.isMouseButtonDown())
        fader.setValue (LevelMeter::gainToDecebel (channel.data().gain), juce::dontSendNotification);
}

void StripFader::refreshLive()
{
    const auto data = channel.data();
    const auto recording = data.record && ! data.monitor;
    auto bus = channel.context.engine->getAudioBusInterface();
    meter.setGrayscale (recording);
    meter.setLevel (recording ? bus->getRecordingLevel (busChannel) : bus->getChannelLevel (busChannel));
}

void StripFader::resized()
{
    // the fader and the meter each stand at their own place from MixerLayout (the slider centres its slot in
    // its width; the meter's scale comes first, then its bar). The two may overlap: the meter paints only its
    // slot and scale, and takes no mouse.
    fader.setBounds (faderX, faderY, faderWidth, faderHeight);
    meter.setBounds (meterX, meterY, LevelMeter::scaleWidth + meterWidth, meterHeight);
}

//==============================================================================
StripName::StripName (StripChannel channel_) : channel (std::move (channel_))
{
    setRepaintsOnMouseActivity (true);
}

void StripName::refresh()
{
    name = "ch " + juce::String (channel.channelNumber + 1);
    repaint();
}

void StripName::paint (juce::Graphics& g)
{
    // the track's colour under the name, as the colour bar of a Pro Tools strip
    const auto colour = channel.track->getViewState().getColour();
    auto bounds = getLocalBounds();
    const auto bar = bounds.removeFromBottom (colourBarHeight);
    g.setColour (colour.withAlpha (0.8f));
    g.fillRect (bar);
    g.setColour (colour.withAlpha (0.18f));
    g.fillRect (bounds);

    g.setColour (findColour (audium::defaultTextColourId));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (name, bounds, juce::Justification::centred, true);
}

void StripName::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || channel.channel() == nullptr)
        return;

    auto& track = *channel.track;
    const auto channelNumber = channel.channelNumber;
    if (! e.mods.isAnyModifierKeyDown())
        track.getSelectionManager()->deselectAll();

    auto rows = track.audioChannelContainer->getSelectedRows();
    if (e.mods.isCommandDown() && rows.contains (channelNumber))
        rows.removeRange ({ channelNumber, channelNumber + 1 });
    else
        rows.addRange ({ channelNumber, channelNumber + 1 });
    track.audioChannelContainer->setSelectedRows (rows);

    // updateAll, as a track header click: the click deselected the track, and the right panel's selected-track header follows that too
    channel.context.container().sendActionMessage (audium::updateAll);
}
