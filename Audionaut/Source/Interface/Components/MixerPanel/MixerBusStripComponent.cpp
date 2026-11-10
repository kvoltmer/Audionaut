//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerBusStripComponent.h"

#include "Interface/ColourIds.h"
#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Engine/Playback/AudioBusInterface.h"

using namespace audium;
using namespace MixerLayout;

//==============================================================================
/// The bus's name on a tint of its colour over the colour bar, as a channel's name bar; an aux's is edited
/// in place on double-click and has a menu on right-click, the master's is fixed.
class MixerBusStripComponent::NameBar : public juce::Label
{
public:
    NameBar (MixerBusStripComponent& owner_, juce::Colour colour_) :
        owner (owner_),
        colour (colour_)
    {
        setJustificationType (juce::Justification::centred);
        setFont (juce::FontOptions (11.0f));
        setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::backgroundWhenEditingColourId, findColour (audium::backgroundColourId));
        setBorderSize (juce::BorderSize<int> (0, 2, colourBarHeight, 2));
        setEditable (false, ! owner.isMaster(), false);
        onTextChange = [this] { owner.rename (getText()); };
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.setColour (colour.withAlpha (0.8f));
        g.fillRect (bounds.removeFromBottom (colourBarHeight));
        g.setColour (colour.withAlpha (0.18f));
        g.fillRect (bounds);
        juce::Label::paint (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) {
            if (! owner.isMaster())
                owner.showNameMenu();
            return;
        }
        juce::Label::mouseDown (e);
    }

private:
    MixerBusStripComponent& owner;
    const juce::Colour colour;
};

//==============================================================================
MixerBusStripComponent::MixerBusStripComponent (MixerContext context_, int auxId_) :
    context (std::move (context_)),
    auxId (auxId_)
{
    inserts = std::make_unique<InsertSlotListComponent> (context, address(), [this] (int index)
    {
        juce::String effectName;
        if (auto* chain = context.chain (address()))
            if (auto slot = chain->getSlot (index))
                effectName = slot->getEffect().getType().name;
        return busName() + " / " + effectName;
    });
    insertsRoom.hold (*inserts);
    addAndMakeVisible (insertsRoom);

    // the pan, a dial like a channel's: the return's, or the master's balance
    ChannelActions::configurePanSlider (&pan);
    pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setRotaryParameters (AudiumLookAndFeel::dialStartAngle, AudiumLookAndFeel::dialEndAngle, true);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pan.setMouseDragSensitivity (120);
    pan.onDragStart = [this] { beginValueEdit(); };
    pan.onValueChange = [this]
    {
        const auto value = static_cast<float> (pan.getValue());
        if (isMaster())
            context.container().setMasterPan (value);
        else if (auto b = bus())
            b->setPan (value);
    };
    pan.onDragEnd = [this] { endValueEdit (isMaster() ? "Set Master Pan" : "Set Aux Pan"); };

    // "Pan" over the dial, captioned as a channel's pan
    panRoom.addAndMakeVisible (pan);
    panRoom.layOut = [this] (juce::Rectangle<int> full) { pan.setBounds (full.withTrimmedTop (knobNameHeight)); };
    panRoom.paintBehind = [this] (juce::Graphics& g, juce::Rectangle<int> full)
    {
        g.setColour (findColour (audium::defaultTextColourId).withAlpha (0.85f));
        g.setFont (juce::FontOptions (8.0f));
        g.drawText (TRANS ("Pan"), full.withHeight (knobNameHeight), juce::Justification::centred, false);
    };
    addAndMakeVisible (panRoom);

    // solo (an aux only) and mute, console keys as the channel strips': raised over the shadows their room draws
    for (auto* button : { &solo, &mute }) {
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
        button->setClickingTogglesState (true);
        button->getProperties().set (AudiumLookAndFeel::consoleButtonProperty, true);
        button->onStateChange = [this] { buttonsRoom.repaint(); };
        buttonsRoom.addAndMakeVisible (button);
    }
    buttonsRoom.layOut = [this] (juce::Rectangle<int> full)
    {
        // solo over mute, each centred, in the channel strips' two button rows
        full.removeFromTop (buttonGap);
        solo.setBounds (full.removeFromTop (buttonRowHeight).withSizeKeepingCentre (buttonWidth, buttonRowHeight));
        full.removeFromTop (buttonGap);
        mute.setBounds (full.removeFromTop (buttonRowHeight).withSizeKeepingCentre (buttonWidth, buttonRowHeight));
    };
    buttonsRoom.paintBehind = [this] (juce::Graphics& g, juce::Rectangle<int>) { AudiumLookAndFeel::drawConsoleButtonShadows (g, { &solo, &mute }); };
    addAndMakeVisible (buttonsRoom);
    solo.setColour (juce::TextButton::buttonOnColourId, findColour (audium::soloColourId));
    mute.setColour (juce::TextButton::buttonOnColourId, findColour (audium::muteColourId));
    solo.onClick = [this]
    {
        beginValueEdit();
        if (auto b = bus())
            b->setSolo (solo.getToggleState());
        endValueEdit ("Aux Solo");
    };
    mute.onClick = [this]
    {
        beginValueEdit();
        if (isMaster())
            context.container().setMasterMute (mute.getToggleState());
        else if (auto b = bus())
            b->setMute (mute.getToggleState());
        endValueEdit (isMaster() ? "Master Mute" : "Aux Mute");
    };

    AudiumLookAndFeel::configureFaderSlider (&fader);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.onDragStart = [this] { beginValueEdit(); };
    fader.onValueChange = [this]
    {
        const auto gain = juce::Decibels::decibelsToGain (static_cast<float> (fader.getValue()),
                                                          static_cast<float> (AudiumLookAndFeel::FaderTaper::minimumDecibels));
        if (isMaster())
            context.container().setMasterGain (gain);
        else if (auto b = bus())
            b->setGain (gain);
    };
    fader.onDragEnd = [this] { endValueEdit (isMaster() ? "Set Master Gain" : "Set Aux Gain"); };
    addAndMakeVisible (fader);

    for (auto* meter : { &meterLeft, &meterRight }) {
        meter->setInterceptsMouseClicks (false, false);   // they may overlap the fader's cap
        addAndMakeVisible (meter);
    }
    meterLeft.setScaleShown (true);

    name = std::make_unique<NameBar> (*this, busColour (auxId));
    addAndMakeVisible (name.get());

    refresh();
}

MixerBusStripComponent::~MixerBusStripComponent() = default;

juce::Colour MixerBusStripComponent::busColour (int auxId) noexcept
{
    return auxId == masterBus ? juce::Colour (0xffb08a4a) : juce::Colour (0xff5b8fa8);
}

std::shared_ptr<AuxBus> MixerBusStripComponent::bus() const
{
    return isMaster() ? nullptr : context.container().getAuxBuses().get (auxId);
}

InsertChainAddress MixerBusStripComponent::address() const
{
    return isMaster() ? InsertChainAddress::ofMaster() : InsertChainAddress::ofAux (auxId);
}

juce::String MixerBusStripComponent::busName() const
{
    if (isMaster())
        return TRANS ("Main");
    auto b = bus();
    return b != nullptr ? juce::String (b->getName()) : juce::String();
}

void MixerBusStripComponent::setShown (const Shown& newShown)
{
    if (shown == newShown)
        return;
    shown = newShown;
    resized();
}

void MixerBusStripComponent::refresh()
{
    const auto b = bus();
    if (! isMaster() && b == nullptr)
        return;   // gone: the group is rebuilt

    inserts->refresh();
    name->setText (busName(), juce::dontSendNotification);

    const auto minimum = static_cast<float> (AudiumLookAndFeel::FaderTaper::minimumDecibels);
    if (isMaster()) {
        auto& container = context.container();
        pan.setValue (container.getMasterPan(), juce::dontSendNotification);
        mute.setToggleState (container.getMasterMute(), juce::dontSendNotification);
        if (! fader.isMouseButtonDown())
            fader.setValue (juce::Decibels::gainToDecibels (container.getMasterGain(), minimum), juce::dontSendNotification);
    }
    else {
        pan.setValue (b->getPan(), juce::dontSendNotification);
        auto anySolo = false;
        for (const auto& other : context.container().getAuxBuses().getBuses())
            anySolo = anySolo || other->getSolo();
        solo.setToggleState (b->getSolo(), juce::dontSendNotification);
        mute.setEnabled (! anySolo);
        mute.setToggleState (b->getMute() || (anySolo && ! b->getSolo()), juce::dontSendNotification);
        if (! fader.isMouseButtonDown())
            fader.setValue (juce::Decibels::gainToDecibels (b->getGain(), minimum), juce::dontSendNotification);
    }
    repaint();
}

void MixerBusStripComponent::refreshLive()
{
    const auto b = bus();
    for (int c = 0; c < 2; ++c) {
        auto level = 0.0f;
        if (isMaster())
            level = context.engine->getAudioBusInterface()->getMasterLevel (c);
        else if (b != nullptr)
            level = b->getLevel (c);
        (c == 0 ? meterLeft : meterRight).setLevel (level);
    }
}

void MixerBusStripComponent::resized()
{
    // under the band, as a track group stacks it: the I/O rows' room (a return has no routing), then the inserts
    auto bounds = getLocalBounds();
    bounds.removeFromTop (ioHeight (shown));
    auto insertsArea = bounds.removeFromTop (insertsHeight (shown));
    insertsArea.removeFromTop (juce::jmin (1, insertsArea.getHeight()));
    insertsArea.removeFromBottom (juce::jmin (1, insertsArea.getHeight()));
    insertsRoom.setRoom (insertsArea, InsertSlotListComponent::heightForRows (insertRows));
    stripTop = bounds.getY();

    // then the strip where MixerLayout::placeStrip puts a channel strip's elements, so the faders line up: the
    // sends' room stays empty (a return has none), the rest takes the channel strip's places
    const auto placement = placeStrip (bounds, shown, static_cast<int> (context.container().getAuxBuses().size()));
    name->setBounds (placement.nameArea);
    panRoom.setRoom (placement.panArea, panHeight);
    solo.setVisible (! isMaster());   // the master has no solo: nothing to solo it against
    buttonsRoom.setRoom (placement.buttonsArea, buttonsHeight);

    auto faderArea = placement.faderArea;
    faderArea.removeFromTop (faderY);
    fader.setBounds (faderArea.withX (faderArea.getX() + faderX).withWidth (faderWidth));

    // the stereo meter: the scale, the left bar and the right bar, both bars lined up with the fader's travel
    const auto indent = getLookAndFeel().getSliderThumbRadius (fader);
    auto meters = faderArea.withX (faderArea.getX() + meterX).withWidth (LevelMeter::scaleWidth + 2 * stereoBarWidth);
    meterLeft.setBounds (meters.removeFromLeft (LevelMeter::scaleWidth + stereoBarWidth).reduced (0, indent - meterLeft.getBarInset()));
    meterRight.setBounds (meters.reduced (0, indent - meterRight.getBarInset()));
}

void MixerBusStripComponent::paint (juce::Graphics& g)
{
    // the group's ground under the inserts, the strip's darker plate from there down, as a track group
    g.fillAll (findColour (audium::secondaryBackgroundColourId));
    g.setColour (findColour (audium::backgroundColourId).darker (0.35f));
    g.fillRect (getLocalBounds().withTop (stripTop));
}

//==============================================================================
void MixerBusStripComponent::beginValueEdit()
{
    if (isMaster())
        masterValuesAtEditStart = UndoableMasterAction::Values::of (context.container());
    else if (auto b = bus())
        auxValuesAtEditStart = UndoableAuxAction::Values::of (*b);
}

void MixerBusStripComponent::endValueEdit (const juce::String& transactionName)
{
    if (isMaster()) {
        const auto newValues = UndoableMasterAction::Values::of (context.container());
        if (newValues == masterValuesAtEditStart)
            return;
        context.undoManager().beginNewTransaction();
        context.undoManager().perform (new UndoableMasterAction (context.container(), masterValuesAtEditStart, newValues), transactionName);
        return;
    }
    auto b = bus();
    if (b == nullptr)
        return;
    const auto newValues = UndoableAuxAction::Values::of (*b);
    const auto& old = auxValuesAtEditStart;
    if (juce::exactlyEqual (newValues.gain, old.gain) && juce::exactlyEqual (newValues.pan, old.pan) && newValues.mute == old.mute && newValues.solo == old.solo)
        return;
    context.undoManager().beginNewTransaction();
    context.undoManager().perform (new UndoableAuxAction (context.container(), auxId, old, newValues), transactionName);
}

void MixerBusStripComponent::rename (const juce::String& newName)
{
    auto b = bus();
    const auto trimmed = newName.trim();
    if (b == nullptr || trimmed.isEmpty() || trimmed == juce::String (b->getName())) {
        refresh();   // back to the name as it is
        return;
    }
    applyAsUndoableEdit (context.container(), [&] { b->setName (trimmed.toStdString()); return true; }, "Rename Aux Bus", false);   // the send knobs re-read the name on its broadcast
}

void MixerBusStripComponent::showNameMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, TRANS ("Rename..."));
    menu.addItem (2, TRANS ("Remove Aux Bus"));
    menu.showMenuAsync (MixerContext::menuOptions (name.get()),
                        [safeThis = juce::Component::SafePointer<MixerBusStripComponent> (this), ctx = context, id = auxId] (int result)
    {
        if (result == 1) {
            if (safeThis != nullptr)
                safeThis->name->showEditor();
        }
        else if (result == 2) {
            // the sends into the bus go with it, and come back with it through undo
            applyAsUndoableEdit (ctx.container(), [&] { return ctx.container().removeAuxBus (id); }, "Remove Aux Bus", false);
        }
    });
}
