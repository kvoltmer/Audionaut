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
    // strip menu: Waveform Size / Routing, behind the chevron the combo boxes wear
    {
        juce::Path chevron;
        chevron.startNewSubPath (4.0f, 5.5f);
        chevron.lineTo (7.5f, 9.5f);
        chevron.lineTo (11.0f, 5.5f);
        juce::DrawablePath image;
        image.setPath (chevron);
        image.setFill (juce::Colours::transparentBlack);
        image.setStrokeFill (findColour (juce::ComboBox::arrowColourId).withAlpha (0.9f));
        image.setStrokeThickness (2.0f);
        menuButton.setImages (&image);
    }
    menuButton.setColour (juce::TextButton::buttonColourId, findColour (juce::ComboBox::backgroundColourId));
    menuButton.setTooltip (TRANS ("Waveform Size / Routing"));
    menuButton.onClick = [this] { showStripMenu(); };
    addAndMakeVisible (menuButton);

    // gain bar
    AudiumLookAndFeel::configureVolumeSlider (&volumeSlider, 12.0);
    volumeSlider.onValueChange = [this] { audioTrack->setGain (Decibels::decibelsToGain (static_cast<float> (volumeSlider.getValue())), rowNumber); };
    volumeSlider.onDragStart = [this] { audioTrack->onDragStart (rowNumber); };
    volumeSlider.onDragEnd = [this] { audioTrack->onDragEnd ("Set Gain"); };
    addAndMakeVisible (volumeSlider);
    
    // pan bar
    configurePanSlider (&panSlider);
    panSlider.onValueChange = [this] { audioTrack->setPan (static_cast<float> (panSlider.getValue()), rowNumber); };
    panSlider.onDragStart = [this] { audioTrack->onDragStart (rowNumber); };
    panSlider.onDragEnd = [this] { audioTrack->onDragEnd ("Set Pan"); };
    addAndMakeVisible (panSlider);

    // Routing indication: only visible when the channel is routed away
    // from the defaults; red when the current device cannot provide the
    // route. Sits under the pan slider.
    for (auto* label : { &inputRoutingLabel, &outputRoutingLabel }) {
        label->setFont (juce::FontOptions (10.0f));
        label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    }
    inputRoutingLabel.setJustificationType (juce::Justification::centredLeft);
    outputRoutingLabel.setJustificationType (juce::Justification::centredRight);

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
    recordButton.onClick = [this] { setRecordEnabled (engine, audioTrack, rowNumber, recordButton.getToggleState()); };
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
    const auto barWidth = meterX - 2 * L::padding;

    auto x = L::padding;
    for (auto* key : std::initializer_list<juce::Component*> { &menuButton, &recordButton, &monitorButton, &soloButton, &muteButton }) {
        key->setBounds (x, L::keysY, L::keySize, L::keySize);
        x += L::keySize + L::padding;
    }

    volumeSlider.setBounds (L::padding, L::gainY, barWidth, L::barHeight);
    panSlider.setBounds (L::padding, L::panY, barWidth, L::barHeight);
    inputRoutingLabel.setBounds (L::padding, L::routingY, barWidth / 2, L::routingHeight);
    outputRoutingLabel.setBounds (L::padding + barWidth / 2, L::routingY, barWidth - barWidth / 2, L::routingHeight);

    // a row too short for a control leaves it out rather than cutting through it: micro shows the keys,
    // small adds the gain bar
    const auto fits = [this] (const juce::Component& control) { return control.getBottom() <= getHeight() - 2; };
    volumeSlider.setVisible (fits (volumeSlider));
    panSlider.setVisible (fits (panSlider));
    routingFits = fits (inputRoutingLabel);
    updateRoutingIndicator();

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
    tick (centreY, 8.0f, 0.5f);
    for (const auto& mark : { Mark { 1.0f, 7.0f, 0.5f }, Mark { 0.5f, 4.0f, 0.35f }, Mark { 0.25f, 2.0f, 0.35f }, Mark { 0.75f, 2.0f, 0.35f } })
        for (const auto sign : { -1.0f, 1.0f })
            tick (centreY + sign * mark.amplitude * fullScale, mark.length, mark.alpha);
}

void ChannelComponent::refreshComponent(std::shared_ptr<audium::AudioTrack> audioTrack_, int rowNumber_, bool isRowSelected)
{
    audioTrack = audioTrack_;
    rowNumber = rowNumber_;
    
    auto channelData = audioTrack->getChannelData(rowNumber);
    
    volumeSlider.setValue(LevelMeter::gainToDecebel(channelData.gain), dontSendNotification);
    panSlider.setValue(channelData.pan, dontSendNotification);
    
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
    

    updateRoutingIndicator();

    if (not isTimerRunning()) {
        startTimerHz(AudiumLookAndFeel::timerHz);
    }
    channelNumber = audioTrack->getChannel(rowNumber)->getChannelNumber() + audioTrack->getChannelOffset();
}

void ChannelComponent::updateRoutingIndicator()
{
    const auto channelData = audioTrack->getChannelData(rowNumber);
    auto* device = engine->getAudioDeviceManager()->getCurrentAudioDevice();

    const auto normalColour = getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.7f);

    auto apply = [this, &normalColour] (juce::Label& label, int routedChannel,
                                        const juce::String& prefix, const juce::StringArray& names)
    {
        if (routedChannel < 0 || ! routingFits) {
            label.setVisible (false);
            return;
        }

        const auto available = routedChannel < names.size();

        label.setText (prefix + " " + juce::String (routedChannel + 1), juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, available ? normalColour : juce::Colours::red);
        label.setTooltip (available ? names[routedChannel]
                                    : prefix + " " + juce::String (routedChannel + 1) + TRANS (" (not available)"));
        label.setVisible (true);
    };

    apply (inputRoutingLabel, channelData.inputChannel, TRANS ("In"),
           device != nullptr ? device->getInputChannelNames() : juce::StringArray());
    apply (outputRoutingLabel, channelData.outputChannel, TRANS ("Out"),
           device != nullptr ? device->getOutputChannelNames() : juce::StringArray());

    // A direct out bypasses the pan into the main bus, so the pan slider
    // has nothing to control.
    panSlider.setEnabled (channelData.outputChannel < 0);
}

void ChannelComponent::showStripMenu()
{
    const auto currentHeight = audioTrack->getChannel(rowNumber)->getChannelHeight();
    const auto safeThis = Component::SafePointer<ChannelComponent> (this);

    // the waveform sizes (AudiumLookAndFeel::SizeIds, the height itself)
    PopupMenu sizeMenu;
    const auto addSize = [&] (const String& name, int height)
    {
        sizeMenu.addItem (name, true, currentHeight == height, [safeThis, height]
        {
            if (safeThis != nullptr)
                safeThis->setChannelHeight (height);
        });
    };
    addSize (TRANS ("micro"),  AudiumLookAndFeel::SizeIds::micro);
    addSize (TRANS ("small"),  AudiumLookAndFeel::SizeIds::small);
    addSize (TRANS ("medium"), AudiumLookAndFeel::SizeIds::medium);
    addSize (TRANS ("large"),  AudiumLookAndFeel::SizeIds::large);
    addSize (TRANS ("huge"),   AudiumLookAndFeel::SizeIds::huge);

    // the routes as the mixer strip offers them
    PopupMenu routing;
    routing.addSubMenu(TRANS("Input"), routingMenu (engine, audioTrack, rowNumber, true));
    routing.addSubMenu(TRANS("Output"), routingMenu (engine, audioTrack, rowNumber, false));

    PopupMenu m;
    m.addSubMenu(TRANS("Waveform Size"), sizeMenu);
    m.addSubMenu(TRANS("Routing"), routing);

    m.setLookAndFeel (&getLookAndFeel());
    m.showMenuAsync (PopupMenu::Options()
                         .withStandardItemHeight(AudiumLookAndFeel::popupMenuItemHeight)
                         .withTargetComponent(&menuButton));
}

void ChannelComponent::setChannelHeight(int height)
{
    audium::applyAsUndoableEdit(audioTrack->getAudioTrackContainer(), [this, height]
    {
        audioTrack->getChannel(rowNumber)->setChannelHeight(height);
        return true;
    },
    "Set audio track height", false);
}

void ChannelComponent::setRoutingChannel (const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber, bool isInput, int newChannel)
{
    audium::applyAsUndoableEdit(audioTrack->getAudioTrackContainer(), [&]
    {
        auto channel = audioTrack->getChannel (channelNumber);
        if (channel == nullptr)
            return false;
        if (isInput)
            channel->setInputChannel(newChannel);
        else
            channel->setOutputChannel(newChannel);
        return true;
    },
    "Set channel routing", false);   // the strips re-read the route on its broadcast
}

juce::PopupMenu ChannelComponent::routingMenu (const std::shared_ptr<audium::AudiumEngine>& engine,
                                               const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber, bool input)
{
    const auto channelData = audioTrack->getChannelData (channelNumber);
    const auto selected = input ? channelData.inputChannel : channelData.outputChannel;

    StringArray names;
    if (auto* device = engine->getAudioDeviceManager()->getCurrentAudioDevice())
        names = input ? device->getInputChannelNames() : device->getOutputChannelNames();

    const auto pick = [audioTrack, channelNumber, input] (int route)
    {
        return [audioTrack, channelNumber, input, route] { setRoutingChannel (audioTrack, channelNumber, input, route); };
    };

    PopupMenu menu;
    menu.addItem (input ? TRANS("Default (input ") + String (channelNumber + 1) + ")" : TRANS("Main Bus"), true, selected < 0, pick (-1));
    for (auto i = 0; i < names.size(); ++i) {
        const auto name = names[i].trim();
        menu.addItem (name.isNotEmpty() ? name : String (i + 1), true, selected == i, pick (i));
    }
    if (selected >= names.size())
        menu.addItem ((input ? TRANS("Input ") : TRANS("Output ")) + String (selected + 1) + TRANS(" (not available)"), false, true, nullptr);
    return menu;
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

    // Once a second: the routing indication depends on the audio device,
    // which can change without this component being refreshed.
    if (++routingRefreshTick >= AudiumLookAndFeel::timerHz) {
        routingRefreshTick = 0;
        updateRoutingIndicator();
    }
}

void ChannelComponent::configurePanSlider(juce::Slider *slider)
{
    slider->setSliderStyle(juce::Slider::LinearBar);
    slider->setColour(Slider::textBoxTextColourId, juce::Colours::white);
    slider->setColour(Slider::trackColourId, juce::Colours::grey.withAlpha(0.5f));
    
    slider->setDoubleClickReturnValue(true, 0.0);
    
    slider->setNormalisableRange(NormalisableRange<double>(-1.0, 1.0));

    slider->textFromValueFunction = [](auto val) {
        auto intVal = static_cast<int>(val * 50.0);
        if (intVal == 0) {
            return String("C");
        }
        else if (intVal < 0) {
            return String(abs(intVal)) + String(" L");
        }
        else if (intVal > 0) {
            return String(intVal) + String(" R");
        }
        return String("");
    };
    slider->updateText();
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

bool ChannelComponent::audioInputAvailable (const std::shared_ptr<audium::AudiumEngine>& engine,
                                            const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber)
{
    auto currentDevice = engine->getAudioDeviceManager()->getCurrentAudioDevice();
    if (currentDevice == nullptr)
        return false;

    auto channelData = audioTrack->getChannelData(channelNumber);
    auto effectiveInput = channelData.inputChannel >= 0 ? channelData.inputChannel : channelNumber;

    return effectiveInput < currentDevice->getActiveInputChannels().countNumberOfSetBits();
}

void ChannelComponent::setRecordEnabled (const std::shared_ptr<audium::AudiumEngine>& engine,
                                         const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber, bool bEnabled)
{
    auto& container = *engine->getAudioTrackContainer();
    auto stepRecorded = false;

    if (audioInputAvailable(engine, audioTrack, channelNumber)) {

        // Undo: store old state
        auto action = bEnabled ? std::make_unique<audium::UndoableContainerAction>(container, false) : nullptr;
        
        if (ModifierKeys::currentModifiers.isShiftDown())
            audioTrack->setRecordEnabled(-1, bEnabled);
        else
            audioTrack->setRecordEnabled(channelNumber, bEnabled);
        
        auto scheduler = engine->getPlayListScheduler();
        
        // start recording on the fly ?
        if (scheduler->isPlaying() &&
            scheduler->isRecordingArmed()) {
            
            if (bEnabled)
                scheduler->startRecording(channelNumber + audioTrack->getChannelOffset());
            else
                scheduler->stopRecording(channelNumber + audioTrack->getChannelOffset());
        }
        
        // Undo: store new state; applying it tells the views
        if (action != nullptr) {
            action->storeNewState();
            container.getUndoManager()->perform(action.release(), "Record");
            container.getUndoManager()->beginNewTransaction();
            stepRecorded = true;
        }
    }
    else {
        if (bEnabled) {
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "No audio input available at this channel.",
                                                        "Please check: Settings ... -> Audio Device Settings");
        }
    }

    // a disarm records no step and a refused arm changed nothing, so no broadcast came from the container:
    // the arrangement's channel strip and the mixer strip both show the arm, and the one not clicked (or a
    // button the engine refused) re-reads the channel's state here
    if (! stepRecorded)
        container.sendActionMessage (audium::updateChannelsAction);
}
