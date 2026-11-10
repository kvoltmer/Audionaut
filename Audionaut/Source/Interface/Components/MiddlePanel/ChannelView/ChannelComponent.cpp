//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ChannelComponent.h"

#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Util/EngineAccess.h"
#include "Engine/PlayList/PlayListScheduler.h"

ChannelComponent::ChannelComponent (std::shared_ptr<audium::AudioTrack> audioTrack_,
                                    std::shared_ptr<audium::AudiumEngine> engine_,
                                    int rowNumber_) :
    audioTrack(audioTrack_),
    engine(engine_),
    rowNumber(rowNumber_)
{
    // RECORD
    {
        juce::Path dot;
        dot.addEllipse (0, 0, 5, 5);
        juce::DrawablePath image;
        image.setPath (dot);
        image.setFill (FillType (Colours::red.darker().darker()));
        recordButton.setImages (&image);
    }
    recordButton.setClickingTogglesState (true);
    recordButton.setColour (TextButton::buttonOnColourId, Colours::red.brighter().withAlpha (0.8f));
    recordButton.setColour (TextButton::buttonColourId, Colours::grey);
    recordButton.onClick = [this] { ChannelActions::setRecordEnabled (engine, audioTrack, rowNumber, recordButton.getToggleState()); };
    addAndMakeVisible (recordButton);

    // MONITOR
    monitorButton.setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
    monitorButton.setColour (juce::TextButton::buttonOnColourId, findColour (audium::soloColourId));
    monitorButton.setClickingTogglesState (true);
    monitorButton.onClick = [this] {
        audioTrack->onDragStart (rowNumber);
        auto data = audioTrack->getChannelData (rowNumber);
        data.monitor = monitorButton.getToggleState();
        audioTrack->setChannelData (rowNumber, data);
        audioTrack->onDragEnd ("Monitor");
    };
    addAndMakeVisible (monitorButton);
    
    // SOLO
    soloButton.setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
    soloButton.setColour (juce::TextButton::buttonOnColourId, findColour (audium::soloColourId));
    soloButton.setClickingTogglesState (true);
    soloButton.onClick = [this] {
        audioTrack->onDragStart (rowNumber);
        audioTrack->setSolo (soloButton.getToggleState(), rowNumber);
        audioTrack->onDragEnd ("Solo");
    };
    addAndMakeVisible (soloButton);

    // MUTE
    muteButton.setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
    muteButton.setColour (juce::TextButton::buttonOnColourId, findColour (audium::muteColourId));
    muteButton.setClickingTogglesState (true);
    muteButton.onClick = [this] {
        audioTrack->onDragStart (rowNumber);
        audioTrack->setMute (muteButton.getToggleState(), rowNumber);
        audioTrack->onDragEnd ("Mute");
    };
    addAndMakeVisible (muteButton);

    // console keys, as on the mixer strip: the shadow under a raised one is painted here, so a change of state
    // repaints the strip
    for (auto* button : std::initializer_list<juce::Button*> { &recordButton, &monitorButton, &soloButton, &muteButton }) {
        button->getProperties().set (AudiumLookAndFeel::consoleButtonProperty, true);
        button->onStateChange = [this] { repaint(); };
    }

    // a route the current device cannot provide, in red under the keys; nothing while the routes are there
    routingWarning.setFont (juce::FontOptions (10.0f));
    routingWarning.setColour (juce::Label::textColourId, juce::Colours::red);
    routingWarning.setJustificationType (juce::Justification::centredLeft);
    routingWarning.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (routingWarning);

    levelMeter.setInterceptsMouseClicks (false, false);   // the row takes the clicks, for the list's selection
    addAndMakeVisible (levelMeter);

    setSize (AudiumLookAndFeel::channelsWidth, AudiumLookAndFeel::SizeIds::medium);
  
    startTimerHz(AudiumLookAndFeel::timerHz);
    refreshComponent(audioTrack, rowNumber, false);
}

void ChannelComponent::resized()
{
    using L = Layout;

    // down the right edge: the meter's bar, then the waveform scale
    const auto meterX = getWidth() - L::scaleWidth - L::meterGap - L::meterWidth;

    // the keys, except in a micro row: too short for them, it shows the meter and the scale only
    const auto keysShown = getHeight() > AudiumLookAndFeel::SizeIds::micro;
    auto x = L::padding;
    for (auto* key : std::initializer_list<juce::Component*> { &recordButton, &monitorButton, &soloButton, &muteButton }) {
        key->setBounds (x, L::keysY, L::keySize, L::keySize);
        key->setVisible (keysShown);
        x += L::keySize + L::padding;
    }

    routingWarning.setBounds (L::padding, L::warningY, meterX - 2 * L::padding, L::warningHeight);
    warningFits = keysShown && routingWarning.getBottom() <= getHeight() - 2;
    updateRoutingWarning();

    levelMeter.setBounds (meterX, L::meterInset, L::meterWidth, getHeight() - 2 * L::meterInset);
}

ChannelComponent::~ChannelComponent()
{
    stopTimer();
}

bool ChannelComponent::isChannelSelected() const
{
    if (audioTrack->getChannel(rowNumber) != nullptr &&
        audioTrack->getChannel(rowNumber)->isSelected()) {
        return true;
    }
    return false;
}


void ChannelComponent::paint (juce::Graphics& g)
{
    
    // background
    auto bgColour = getLookAndFeel().findColour (audium::secondaryBackgroundColourId);
    if (isChannelSelected()) {
        bgColour = bgColour.brighter().withAlpha(0.25f);
    }
    g.fillAll (bgColour);
    
    // border
    auto borderColour = juce::Colours::black.withAlpha(0.50f);
    g.setColour(borderColour);
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 2.0f);

    AudiumLookAndFeel::drawConsoleButtonShadows (g, { &recordButton, &monitorButton, &soloButton, &muteButton });

    drawWaveformScale (g, getLocalBounds().removeFromRight (Layout::scaleWidth));
    
    g.setColour(audioTrack->getViewState().getColour());
    if( insertAfter )
    {
        g.fillRect(0, getHeight()-3, getWidth(), 3);
    }
    else if( insertBefore )
    {
        g.fillRect(0, 0, getWidth(), 3);
    }

}

void ChannelComponent::drawWaveformScale (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const auto right = static_cast<float> (area.getRight()) - 1.0f;
    const auto centreY = static_cast<float> (area.getCentreY());
    const auto fullScale = static_cast<float> (area.getHeight()) * 0.5f * AudiumLookAndFeel::waveformScale;
    const auto tickColour = getLookAndFeel().findColour (audium::defaultTextColourId);

    const auto tick = [&] (float y, float length, float alpha)
    {
        g.setColour (tickColour.withAlpha (alpha));
        g.fillRect (right - length, y - 0.5f, length, 1.0f);
    };

    // the centre line, then each amplitude above and below it: the quarters short, the half middling, full scale long
    struct Mark { float amplitude, length, alpha; };
    tick (centreY, 6.0f, 0.35f);
    for (const auto& mark : { Mark { 1.0f, 5.0f, 0.35f }, Mark { 0.5f, 3.0f, 0.22f }, Mark { 0.25f, 2.0f, 0.22f }, Mark { 0.75f, 2.0f, 0.22f } })
        for (const auto sign : { -1.0f, 1.0f })
            tick (centreY + sign * mark.amplitude * fullScale, mark.length, mark.alpha);
}

void ChannelComponent::refreshComponent(std::shared_ptr<audium::AudioTrack> audioTrack_, int rowNumber_, bool isRowSelected)
{
    audioTrack = audioTrack_;
    rowNumber = rowNumber_;
    
    auto channelData = audioTrack->getChannelData(rowNumber);
    
    
    auto bMute = channelData.mute;
    auto bSolo = channelData.solo;
    
    auto anySolo = audioTrack->getAudioTrackContainer().anyChannelSolo();
    if (anySolo && !bSolo) {
        bMute = true;
    }
    
    muteButton.setEnabled(!anySolo);
    
    muteButton.setToggleState(bMute, dontSendNotification);
    soloButton.setToggleState(bSolo, dontSendNotification);
    
    monitorButton.setToggleState(channelData.monitor, dontSendNotification);
    recordButton.setToggleState(channelData.record, dontSendNotification);
    repaint();   // the button shadows follow the toggle states
    updateRoutingWarning();

    if (not isTimerRunning()) {
        startTimerHz(AudiumLookAndFeel::timerHz);
    }
    channelNumber = audioTrack->getChannel(rowNumber)->getChannelNumber() + audioTrack->getChannelOffset();
}

void ChannelComponent::timerCallback()
{
    auto lvl = 0.f;
    
    auto data = audioTrack->getChannelData(rowNumber);
    auto displayRecordingLevel = (data.record && !data.monitor);
    if (displayRecordingLevel) {
        lvl = engine->getAudioBusInterface()->getRecordingLevel(channelNumber);
    }
    else {
        lvl = engine->getAudioBusInterface()->getChannelLevel(channelNumber);
        jassert(lvl >= 0.f);
    }
    levelMeter.setGrayscale(displayRecordingLevel);
    levelMeter.setLevel(lvl);
    
    if (recordButton.getToggleState() != data.record) {
        recordButton.setToggleState(data.record, dontSendNotification);
        repaint();   // the button's shadow
    }

    // once a second: whether a route is there depends on the audio device, which changes without a refresh
    if (++routingRefreshTick >= AudiumLookAndFeel::timerHz) {
        routingRefreshTick = 0;
        updateRoutingWarning();
    }
}

void ChannelComponent::updateRoutingWarning()
{
    const auto channelData = audioTrack->getChannelData (rowNumber);
    auto* device = engine->getAudioDeviceManager()->getCurrentAudioDevice();
    const auto numInputs = device != nullptr ? device->getInputChannelNames().size() : 0;
    const auto numOutputs = device != nullptr ? device->getOutputChannelNames().size() : 0;

    // only a route chosen by hand can be missing; the defaults always resolve
    juce::StringArray missing;
    if (channelData.inputChannel >= numInputs)
        missing.add (TRANS ("In ") + juce::String (channelData.inputChannel + 1));
    if (channelData.outputChannel >= numOutputs)
        missing.add (TRANS ("Out ") + juce::String (channelData.outputChannel + 1));

    routingWarning.setText (missing.joinIntoString (", "), juce::dontSendNotification);
    routingWarning.setTooltip (missing.isEmpty() ? juce::String() : missing.joinIntoString (", ") + TRANS (" not available on this audio device"));
    routingWarning.setVisible (warningFits && ! missing.isEmpty());
}

void ChannelComponent::mouseDown (const juce::MouseEvent& e)
{
    bool isSelected = isChannelSelected();
    
    if (!e.mods.isAnyModifierKeyDown() && !isSelected) {
        audioTrack->getSelectionManager()->deselectAll();
    }
    
    /// pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(e);
    
    if (e.mods.isPopupMenu()) {
    
        PopupMenu m;
        m.addItem (TRANS ("Copy selected channel(s) to new track"), [safeThis = Component::SafePointer<ChannelComponent> (this)]
        {
            if (safeThis == nullptr)
                return;
            if (!safeThis->getEngine()->getAudioTrackContainer()->copySelectedChannelsToNewTrack()) {
                NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                      "Error",
                                                      "no channel selected.");
            }
        });
        m.setLookAndFeel (&getLookAndFeel());
        m.showMenuAsync (PopupMenu::Options().withStandardItemHeight(AudiumLookAndFeel::popupMenuItemHeight));
    }
    
    // updateAll, as a track header click: selecting a channel deselected the track, and the right panel's
    // selected-track header and the mixer follow that, not only this view and the arrangement
    audioTrack->getAudioTrackContainer().sendActionMessage(audium::updateAll);
}

void ChannelComponent::mouseUp (const juce::MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

bool ChannelComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    return dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get()) != nullptr;
}

void ChannelComponent::updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (auto channelComponent = dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get())) {
        
        auto before = dragSourceDetails.localPosition.y < getHeight() / 2;
        auto insertIndex = rowNumber + (before ? 0 : 1);
                    
        if (getAudioTrack() == channelComponent->getAudioTrack() &&
            (rowNumber == channelComponent->rowNumber ||
             insertIndex == channelComponent->rowNumber)) {
            hideInsertLines();
            return;
        }
        
        if (before) {
            insertBefore = true;
            insertAfter = false;
        }
        else {
            insertAfter = true;
            insertBefore = false;
        }
    }
    repaint();
}

void ChannelComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
    if (auto channelComponent = dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get())) {
        
        auto before = dragSourceDetails.localPosition.y < getHeight() / 2;
        auto insertIndex = rowNumber + (before ? 0 : 1);
        if (getAudioTrack() == channelComponent->getAudioTrack() &&
            (rowNumber == channelComponent->rowNumber ||
             insertIndex == channelComponent->rowNumber)) {
            hideInsertLines();
            return;
        }
        
        
        
        // Undo: store old state
        auto action = std::make_unique<audium::UndoableContainerAction>(getAudioTrack()->getAudioTrackContainer());
        
        if (getAudioTrack() == channelComponent->getAudioTrack()) {
        
            // remember old channel mapping
            std::vector<int> channelNumbers;
            for (auto channel : getAudioTrack()->audioChannelContainer->objects)
                channelNumbers.push_back(channel->getChannelNumber());
            
            audium::MoveItemBefore(getAudioTrack()->audioChannelContainer->objects,
                                   channelComponent->rowNumber,
                                   insertIndex);
            
            audium::MoveItemBefore(channelNumbers,
                                   channelComponent->rowNumber,
                                   insertIndex);
    
            
            // re-mapping destination channels
            for (auto resource : getAudioTrack()->getAudioResources()) {
                auto dst = resource->getChannelMapping().getDestinationChannel();
                auto it = std::find(channelNumbers.begin(), channelNumbers.end(), dst);
                if (it != channelNumbers.end()) {
                    auto newDst = static_cast<int>(std::distance(channelNumbers.begin(), it));
                    resource->getChannelMapping().setDestinationChannel(newDst);
                }
            }
            
        }
        else {
            // TODO: implement
            notImplemented();
        }
        
        // Undo: store new state
        action->storeNewState();
        auto undoManager = engine->getUndoManager();
        engine->getUndoManager()->perform(action.release(), "Channels changed");
        engine->getUndoManager()->beginNewTransaction();
        
    }
    hideInsertLines();
}
