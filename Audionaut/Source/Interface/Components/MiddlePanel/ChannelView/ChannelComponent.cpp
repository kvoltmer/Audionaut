//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ChannelComponent.h"

#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Util/EngineAccess.h"

ChannelComponent::ChannelComponent (std::shared_ptr<audium::AudioTrack> audioTrack_,
                                    std::shared_ptr<audium::AudiumEngine> engine_,
                                    int rowNumber) :
    audioTrack(audioTrack_),
    engine(engine_)
{
    
    levelMeter.reset (new LevelMeter (true, false));
    addAndMakeVisible(levelMeter.get());

    volumeScaleButton.reset (new juce::ImageButton ("volume scale"));
    addAndMakeVisible (volumeScaleButton.get());
    volumeScaleButton->setButtonText (TRANS ("new button"));
    volumeScaleButton->setImages (false, true, false,
                                  juce::ImageCache::getFromMemory (channelScale_png, channelScale_pngSize), 1.000f, juce::Colour (0x00000000),
                                  juce::Image(), 1.000f, juce::Colour (0x00000000),
                                  juce::Image(), 1.000f, juce::Colour (0x00000000));


    
    channelSizeComboBox.reset (new juce::ComboBox ("channel size combo box"));
    addAndMakeVisible (channelSizeComboBox.get());
    channelSizeComboBox->setEditableText (false);
    channelSizeComboBox->setJustificationType (juce::Justification::centred);
    channelSizeComboBox->addItem (TRANS ("small"), 1);
    channelSizeComboBox->addItem (TRANS ("medium"), 2);
    channelSizeComboBox->addItem (TRANS ("large"), 3);
    channelSizeComboBox->addItem (TRANS ("huge"), 4);
    channelSizeComboBox->addListener (this);


    // disable mouse clicks. we need them for the list control
    volumeScaleButton->setInterceptsMouseClicks(false, false);
    
    // volume slider
    volumeSlider = std::make_unique<juce::Slider>();
    addAndMakeVisible(volumeSlider.get());
    configureVolumeSlider(volumeSlider.get());
    volumeSlider->onValueChange = [this, rowNumber] {
        audioTrack->setGain(Decibels::decibelsToGain(volumeSlider->getValue()), rowNumber);
    };
    volumeSlider->onDragStart = [this] {
        audioTrack->onDragStart();
    };
    
    volumeSlider->onDragEnd = [this] {
        audioTrack->onDragEnd();
    };
    
    // pan slider
    panSlider = std::make_unique<juce::Slider>();
    addAndMakeVisible(panSlider.get());
    configurePanSlider(panSlider.get());
    panSlider->onValueChange = [this, rowNumber] {
        audioTrack->setPan(panSlider->getValue(), rowNumber);
    };
    panSlider->onDragStart = [this] {
        audioTrack->onDragStart();
    };
    panSlider->onDragEnd = [this] {
        audioTrack->onDragEnd();
    };
    
    // MUTE
    muteButton.reset (new juce::TextButton ("M"));
    addAndMakeVisible (muteButton.get());
    muteButton->setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
    muteButton->setColour (juce::TextButton::buttonOnColourId, findColour (audium::muteColourId));
    muteButton->setClickingTogglesState(true);
    muteButton->onClick = [this, rowNumber] {
        audioTrack->onDragStart();
        audioTrack->setMute(muteButton->getToggleState(), rowNumber);
        audioTrack->onDragEnd();
    };
    
    // SOLO
    soloButton.reset (new juce::TextButton ("S"));
    addAndMakeVisible (soloButton.get());
    soloButton->setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
    soloButton->setColour (juce::TextButton::buttonOnColourId, findColour (audium::soloColourId));
    soloButton->setClickingTogglesState(true);
    soloButton->onClick = [this, rowNumber] {
        audioTrack->onDragStart();
        audioTrack->setSolo(soloButton->getToggleState(), rowNumber);
        audioTrack->onDragEnd();
    };
    

    setSize (AudiumLookAndFeel::channelsWidth, 100);
    
    startTimerHz(60);
    
    refreshComponent(audioTrack, rowNumber, false);
}

void ChannelComponent::resized()
{
    auto sliderWidth = 67;
    auto sliderHeight = 15;
    auto space = 7;
    
    channelSizeComboBox->setBounds (space, 5, 15, 15);
    
    auto buttonSize = 15;
    muteButton->setBounds(space + 30, 5, buttonSize, buttonSize);
    soloButton->setBounds(space + 53, 5, buttonSize, buttonSize);
    
    volumeSlider->setBounds (space,
                             27,
                             sliderWidth,
                             sliderHeight);
    
    panSlider->setBounds (space,
                          50,
                          sliderWidth,
                          sliderHeight);
    
    levelMeter->setBounds(getWidth() - 20, 3, 7, getHeight() - 6);
    volumeScaleButton->setBounds (getWidth() - 10, 0, 10, proportionOfHeight (1.0000f));
}

ChannelComponent::~ChannelComponent()
{
    stopTimer();
    audioTrack = nullptr;
}

void ChannelComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    if (audioTrack->getChannel(rowNumber) != nullptr &&
        audioTrack->getChannel(rowNumber)->isSelected())
    {
        g.setColour (juce::Colours::white.withAlpha(0.25f));
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha(0.50f));
    }
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 2.0f);
    
    g.setColour(audioTrack->getColour());
    if( insertAfter )
    {
        g.fillRect(0, getHeight()-3, getWidth(), 3);
    }
    else if( insertBefore )
    {
        g.fillRect(0, 0, getWidth(), 3);
    }

}

void ChannelComponent::refreshComponent(std::shared_ptr<audium::AudioTrack> audioTrack_, int rowNumber_, bool isRowSelected)
{
    audioTrack = audioTrack_;
    rowNumber = rowNumber_;
    
    volumeSlider->setValue(LevelMeter::gainToDecebel(audioTrack->getGain(rowNumber)), dontSendNotification);
    panSlider->setValue(audioTrack->getPan(rowNumber), dontSendNotification);
    
    auto bMute = audioTrack->getMute(rowNumber);
    auto bSolo = audioTrack->getSolo(rowNumber);
    auto anySolo = audioTrack->getAudioTrackContainer().anyChannelSolo();
    if (anySolo && !bSolo) {
        bMute = true;
    }
    
    muteButton->setEnabled(!anySolo);
    
    muteButton->setToggleState(bMute, dontSendNotification);
    soloButton->setToggleState(bSolo, dontSendNotification);
    
    if (not isTimerRunning()) {
        startTimerHz(60);
    }
    
    channelNumber = audioTrack->getChannel(rowNumber)->getChannelNumber() + audioTrack->getChannelOffset();
}

void ChannelComponent::timerCallback()
{
    auto lvl = engine->getAudioBusInterface()->getChannelLevel(channelNumber);
    levelMeter->setLevel(lvl);
}

void ChannelComponent::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == channelSizeComboBox.get())
    {
        auto height = 0;
        switch (channelSizeComboBox->getSelectedId()) {
            case 1:
                height = 50;
                break;
            case 2:
                height = 100;
                break;
            case 3:
                height = 200;
                break;
            case 4:
                height = 400;
                break;
            default:
                break;
        }

        channelSizeComboBox->setText("", dontSendNotification);

        // undo
        auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer(), false);
        
        audioTrack->getChannel(rowNumber)->setChannelHeight(height);
        
        // undo
        action->storeNewState();
        audioTrack->getAudioTrackContainer().getUndoManager()->perform(action.release(), "Set audio track height");
        audioTrack->getAudioTrackContainer().getUndoManager()->beginNewTransaction();
    }
}

void ChannelComponent::configureVolumeSlider(juce::Slider *slider, double dbMax)
{
    slider->setSliderStyle(juce::Slider::LinearBarVertical);
    slider->setColour(Slider::textBoxTextColourId, juce::Colours::white);
    slider->setColour(Slider::trackColourId, Colours::transparentBlack);
    
    slider->setTextValueSuffix (" dB");
    slider->setNumDecimalPlacesToDisplay(1);
    slider->setDoubleClickReturnValue(true, 0.0);
    slider->setVelocityModeParameters(1.0, 1, 0.05);
    slider->setVelocityBasedMode(true);
    
    auto scaled2UnscaledFunc = [](auto min, auto max, auto scaled) {
        return scale_linear(pow(scaled, 0.33333333), min, max);
    };
    auto unscaled2ScaledFunc = [](auto min, auto max, auto unscaled) {
        return pow(reverse_linear(unscaled, min, max), 3.0);
    };
    slider->setNormalisableRange(NormalisableRange<double>(-80.0, dbMax,
                                                           scaled2UnscaledFunc,
                                                           unscaled2ScaledFunc));
    
    slider->textFromValueFunction = [](auto val) {
        if (val <= -80.0)
            return String("-") + String(juce::CharPointer_UTF8 ("\xe2\x88\x9e"));
        return String(val, 1);
    };
    slider->updateText();
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

static void channelMenuCallback (int result, ChannelComponent* component, int rowIdClicked)
{
    if (component != nullptr && result != 0)
    {
        switch (result) {
            case ChannelComponent::moveChannelToNewTrackId:
                component->getEngine()->getAudioTrackContainer()->copySelectedChannelsToNewTrack();
                break;

            default:
                break;
        }
    }
}

void ChannelComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) {
    
        PopupMenu m;

        m.addItem (moveChannelToNewTrackId, TRANS ("Copy selected channel(s) to new track"), true);

        if (m.getNumItems() > 0)
        {
            m.setLookAndFeel (&getLookAndFeel());

            m.showMenuAsync (PopupMenu::Options(),
                             ModalCallbackFunction::forComponent (channelMenuCallback, this, rowNumber));
        }
    }
    else {
        
        if (audioTrack->getChannel(rowNumber)->isSelected()) {
            return;
        }
        
        if (!e.mods.isAnyModifierKeyDown()) {
            audioTrack->getSelectionManager()->deselectAll();
        }
        getParentComponent()->mouseDown(e);
        audioTrack->getAudioTrackContainer().sendActionMessage(audium::updateMiddlePanelAction);
    }
}

void ChannelComponent::mouseUp (const juce::MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

bool ChannelComponent::keyPressed (const juce::KeyPress& key)
{
    return false;  // Return true if your handler uses this key event, or false to allow it to be passed-on.
}

bool ChannelComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (auto item = dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get())) {
        //if (item->getPlayListModel() == playListModel)
        {
            // return true if source details match this model
            return true;
        }
    }
    return false;
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
        
            std::cout << "MoveItemBefore -> currentIndex: " << channelComponent->rowNumber << " indexOfItemToPlaceBefore " << insertIndex << std::endl;
            
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

//==============================================================================
// Binary resources - be careful not to edit any of these sections!

// JUCER_RESOURCE: channelScale_png, 3644, "../../../../Resources/channelScale.png"
static const unsigned char resource_ChannelComponent_channelScale_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,14,0,0,0,117,8,6,0,0,0,126,173,208,211,0,0,10,183,105,67,67,80,73,67,67,
32,80,114,111,102,105,108,101,0,0,72,137,149,151,7,80,83,89,23,199,239,123,233,141,150,16,1,41,161,55,65,58,1,164,132,30,186,116,176,17,146,16,66,9,33,5,21,187,178,184,130,107,65,69,4,148,21,93,17,80,
112,45,128,172,21,81,108,139,96,3,235,130,44,2,194,186,88,176,97,249,30,48,132,221,253,230,251,190,249,206,204,157,251,123,231,157,251,63,247,188,185,119,230,60,0,40,70,28,177,56,3,86,1,32,83,36,147,68,
6,120,51,226,19,18,25,184,1,64,0,16,32,1,45,64,228,112,165,98,86,68,68,8,64,108,122,254,187,189,187,143,196,34,118,199,106,66,235,223,223,255,87,83,229,241,165,92,0,160,8,132,147,121,82,110,38,194,39,
145,241,149,43,150,200,0,64,33,12,12,151,202,196,19,220,141,48,77,130,108,16,225,145,9,22,76,50,122,66,135,150,60,197,180,201,152,232,72,31,132,205,0,192,147,57,28,137,0,0,178,3,226,103,228,112,5,136,
14,57,26,97,27,17,79,40,66,56,15,97,143,204,204,44,30,194,173,8,155,33,49,98,132,39,244,153,201,127,209,17,252,77,51,89,161,201,225,8,20,60,85,203,164,225,125,133,82,113,6,103,249,255,249,57,254,183,101,
102,200,167,115,152,32,131,156,42,9,140,156,98,168,59,61,43,88,193,162,228,176,240,105,22,242,166,227,161,238,84,121,96,204,52,115,165,62,137,211,204,227,248,6,43,214,102,132,133,76,115,138,208,159,173,
208,145,177,163,167,153,47,245,139,154,102,73,86,164,34,87,138,196,135,53,205,28,201,76,94,121,122,140,194,159,202,103,43,244,115,83,163,227,166,57,71,24,27,54,205,210,244,168,224,153,24,31,133,95,34,
143,84,236,159,47,10,240,158,201,235,175,168,61,83,250,151,122,133,108,197,90,89,106,116,160,162,118,206,204,254,249,34,214,140,166,52,94,177,55,30,223,215,111,38,38,70,17,47,150,121,43,114,137,51,34,
20,241,252,140,0,133,95,154,19,165,88,43,67,14,228,204,218,8,197,55,76,227,4,69,76,51,136,6,169,64,14,68,128,7,248,64,2,146,65,22,200,0,50,192,0,190,64,8,164,64,140,60,113,0,114,156,100,252,101,178,137,
226,124,178,196,203,37,66,65,170,140,193,66,110,29,159,193,22,113,173,231,48,236,108,236,236,1,152,184,195,83,71,228,13,125,242,110,66,244,235,51,190,236,11,0,184,20,32,78,193,140,143,99,8,192,233,231,
0,80,223,205,248,12,95,35,199,107,27,0,103,59,184,114,73,206,148,111,242,174,97,0,17,40,3,26,208,4,186,192,16,152,1,43,96,7,156,128,27,240,2,126,32,8,132,35,149,36,128,197,128,139,212,147,137,84,178,20,
172,4,235,64,62,40,4,219,192,46,80,10,42,192,1,112,24,28,5,199,65,35,56,3,46,130,43,224,6,232,0,247,192,35,208,3,250,193,48,24,5,239,192,56,4,65,56,136,2,81,33,77,72,15,50,134,44,33,59,136,9,121,64,126,
80,8,20,9,37,64,73,144,0,18,65,114,104,37,180,1,42,132,138,160,82,104,63,84,13,253,12,157,134,46,66,215,160,78,232,1,212,11,13,65,175,161,79,48,10,38,195,52,88,7,54,129,231,194,76,152,5,7,195,209,240,
34,88,0,103,195,185,112,30,188,5,46,129,43,225,35,112,3,124,17,190,1,223,131,123,224,97,120,12,5,80,36,20,29,165,143,178,66,49,81,62,168,112,84,34,42,5,37,65,173,70,21,160,138,81,149,168,58,84,51,170,
13,117,7,213,131,26,65,125,68,99,209,84,52,3,109,133,118,67,7,162,99,208,92,116,54,122,53,122,51,186,20,125,24,221,128,110,69,223,65,247,162,71,209,95,49,20,140,54,198,18,227,138,97,99,226,49,2,204,82,
76,62,166,24,115,8,115,10,115,25,115,15,211,143,121,135,197,98,233,88,83,172,51,54,16,155,128,77,195,174,192,110,198,238,197,214,99,47,96,59,177,125,216,49,28,14,167,137,179,196,185,227,194,113,28,156,
12,151,143,219,131,59,130,59,143,187,141,235,199,125,192,147,240,122,120,59,188,63,62,17,47,194,175,199,23,227,107,240,231,240,183,241,3,248,113,130,10,193,152,224,74,8,39,240,8,203,9,91,9,7,9,205,132,
91,132,126,194,56,81,149,104,74,116,39,70,19,211,136,235,136,37,196,58,226,101,226,99,226,27,18,137,100,64,114,33,205,39,9,73,107,73,37,164,99,164,171,164,94,210,71,178,26,217,130,236,67,94,72,150,147,
183,144,171,200,23,200,15,200,111,40,20,138,9,197,139,146,72,145,81,182,80,170,41,151,40,79,41,31,148,168,74,214,74,108,37,158,210,26,165,50,165,6,165,219,74,47,149,9,202,198,202,44,229,197,202,185,202,
197,202,39,148,111,41,143,168,16,84,76,84,124,84,56,42,171,85,202,84,78,171,116,169,140,169,82,85,109,85,195,85,51,85,55,171,214,168,94,83,29,84,195,169,153,168,249,169,241,212,242,212,14,168,93,82,235,
163,162,168,134,84,31,42,151,186,129,122,144,122,153,218,79,195,210,76,105,108,90,26,173,144,118,148,214,78,27,85,87,83,119,80,143,85,95,166,94,166,126,86,189,135,142,162,155,208,217,244,12,250,86,250,
113,250,125,250,167,89,58,179,88,179,248,179,54,205,170,155,117,123,214,123,141,217,26,94,26,124,141,2,141,122,141,123,26,159,52,25,154,126,154,233,154,219,53,27,53,159,104,161,181,44,180,230,107,45,213,
218,167,117,89,107,100,54,109,182,219,108,238,236,130,217,199,103,63,212,134,181,45,180,35,181,87,104,31,208,190,169,61,166,163,171,19,160,35,214,217,163,115,73,103,68,151,174,235,165,155,166,187,83,247,
156,238,144,30,85,207,67,79,168,183,83,239,188,222,11,134,58,131,197,200,96,148,48,90,25,163,250,218,250,129,250,114,253,253,250,237,250,227,6,166,6,49,6,235,13,234,13,158,24,18,13,153,134,41,134,59,13,
91,12,71,141,244,140,66,141,86,26,213,26,61,52,38,24,51,141,83,141,119,27,183,25,191,55,49,53,137,51,217,104,210,104,50,104,170,97,202,54,205,53,173,53,125,108,70,49,243,52,203,54,171,52,187,107,142,53,
103,154,167,155,239,53,239,176,128,45,28,45,82,45,202,44,110,89,194,150,78,150,66,203,189,150,157,115,48,115,92,230,136,230,84,206,233,178,34,91,177,172,114,172,106,173,122,173,233,214,33,214,235,173,
27,173,95,206,53,154,155,56,119,251,220,182,185,95,109,28,109,50,108,14,218,60,178,85,179,13,178,93,111,219,108,251,218,206,194,142,107,87,102,119,215,158,98,239,111,191,198,190,201,254,149,131,165,3,
223,97,159,67,183,35,213,49,212,113,163,99,139,227,23,39,103,39,137,83,157,211,144,179,145,115,146,115,185,115,23,147,198,140,96,110,102,94,117,193,184,120,187,172,113,57,227,242,209,213,201,85,230,122,
220,245,79,55,43,183,116,183,26,183,193,121,166,243,248,243,14,206,235,115,55,112,231,184,239,119,239,241,96,120,36,121,252,232,209,227,169,239,201,241,172,244,124,230,101,232,197,243,58,228,53,192,50,
103,165,177,142,176,94,122,219,120,75,188,79,121,191,247,113,245,89,229,115,193,23,229,27,224,91,224,219,238,167,230,23,227,87,234,247,212,223,192,95,224,95,235,63,26,224,24,176,34,224,66,32,38,48,56,
112,123,96,23,91,135,205,101,87,179,71,131,156,131,86,5,181,6,147,131,163,130,75,131,159,133,88,132,72,66,154,67,225,208,160,208,29,161,143,195,140,195,68,97,141,225,32,156,29,190,35,252,73,132,105,68,
118,196,47,243,177,243,35,230,151,205,127,30,105,27,185,50,178,45,138,26,181,36,170,38,234,93,180,119,244,214,232,71,49,102,49,242,152,150,88,229,216,133,177,213,177,239,227,124,227,138,226,122,226,231,
198,175,138,191,145,160,149,32,76,104,74,196,37,198,38,30,74,28,91,224,183,96,215,130,254,133,142,11,243,23,222,95,100,186,104,217,162,107,139,181,22,103,44,62,187,68,121,9,103,201,137,36,76,82,92,82,
77,210,103,78,56,167,146,51,150,204,78,46,79,30,229,250,112,119,115,135,121,94,188,157,188,33,190,59,191,136,63,144,226,158,82,148,50,40,112,23,236,16,12,165,122,166,22,167,142,8,125,132,165,194,87,105,
129,105,21,105,239,211,195,211,171,210,191,101,196,101,212,103,226,51,147,50,79,139,212,68,233,162,214,44,221,172,101,89,157,98,75,113,190,184,39,219,53,123,87,246,168,36,88,114,72,10,73,23,73,155,100,
52,164,89,186,41,55,147,127,39,239,205,241,200,41,203,249,176,52,118,233,137,101,170,203,68,203,110,46,183,88,190,105,249,64,174,127,238,79,43,208,43,184,43,90,86,234,175,92,183,178,119,21,107,213,254,
213,208,234,228,213,45,107,12,215,228,173,233,95,27,176,246,240,58,226,186,244,117,191,174,183,89,95,180,254,237,134,184,13,205,121,58,121,107,243,250,190,11,248,174,54,95,41,95,146,223,181,209,109,99,
197,247,232,239,133,223,183,111,178,223,180,103,211,215,2,94,193,245,66,155,194,226,194,207,155,185,155,175,255,96,251,67,201,15,223,182,164,108,105,223,234,180,117,223,54,236,54,209,182,251,219,61,183,
31,46,82,45,202,45,234,219,17,186,163,97,39,99,103,193,206,183,187,150,236,186,86,236,80,92,177,155,184,91,190,187,167,36,164,164,105,143,209,158,109,123,62,151,166,150,222,43,243,46,171,47,215,46,223,
84,254,126,47,111,239,237,125,94,251,234,42,116,42,10,43,62,253,40,252,177,123,127,192,254,134,74,147,202,226,3,216,3,57,7,158,31,140,61,216,246,19,243,167,234,67,90,135,10,15,125,169,18,85,245,28,142,
60,220,90,237,92,93,93,163,93,179,181,22,174,149,215,14,29,89,120,164,227,168,239,209,166,58,171,186,253,245,244,250,194,99,224,152,252,216,139,159,147,126,190,127,60,248,120,203,9,230,137,186,147,198,
39,203,79,81,79,21,52,64,13,203,27,70,27,83,27,123,154,18,154,58,79,7,157,110,105,118,107,62,245,139,245,47,85,103,244,207,148,157,85,63,187,245,28,241,92,222,185,111,231,115,207,143,93,16,95,24,185,40,
184,216,215,178,164,229,209,165,248,75,119,91,231,183,182,95,14,190,124,245,138,255,149,75,109,172,182,243,87,221,175,158,185,230,122,237,244,117,230,245,198,27,78,55,26,110,58,222,60,245,171,227,175,
167,218,157,218,27,110,57,223,106,234,112,233,104,238,156,215,121,238,182,231,237,139,119,124,239,92,185,203,190,123,227,94,216,189,206,251,49,247,187,187,22,118,245,116,243,186,7,31,100,60,120,245,48,
231,225,248,163,181,143,49,143,11,158,168,60,41,126,170,253,180,242,55,243,223,234,123,156,122,206,246,250,246,222,124,22,245,236,81,31,183,111,248,119,233,239,159,251,243,158,83,158,23,15,232,13,84,15,
218,13,158,25,242,31,234,120,177,224,69,255,176,120,120,124,36,255,15,213,63,202,95,154,189,60,249,167,215,159,55,71,227,71,251,95,73,94,125,123,189,249,141,230,155,170,183,14,111,91,198,34,198,158,190,
203,124,55,254,190,224,131,230,135,195,31,153,31,219,62,197,125,26,24,95,250,25,247,185,228,139,249,151,230,175,193,95,31,127,203,252,246,77,204,145,112,38,91,1,20,50,224,148,20,0,94,87,1,64,73,64,122,
135,14,0,136,11,166,122,236,73,131,166,254,11,38,9,252,39,158,234,195,39,205,9,128,42,47,0,98,214,2,16,130,244,40,251,144,97,140,48,25,153,39,218,164,104,47,0,219,219,43,198,116,63,60,217,187,79,24,22,
249,139,41,50,213,160,43,13,223,170,233,90,11,254,97,83,125,253,95,246,253,207,25,40,84,255,54,255,11,203,133,14,176,234,88,217,93,0,0,0,68,101,88,73,102,77,77,0,42,0,0,0,8,0,2,1,18,0,3,0,0,0,1,0,1,0,
0,135,105,0,4,0,0,0,1,0,0,0,38,0,0,0,0,0,2,160,2,0,4,0,0,0,1,0,0,0,14,160,3,0,4,0,0,0,1,0,0,0,117,0,0,0,0,249,130,125,138,0,0,2,3,105,84,88,116,88,77,76,58,99,111,109,46,97,100,111,98,101,46,120,109,112,
0,0,0,0,0,60,120,58,120,109,112,109,101,116,97,32,120,109,108,110,115,58,120,61,34,97,100,111,98,101,58,110,115,58,109,101,116,97,47,34,32,120,58,120,109,112,116,107,61,34,88,77,80,32,67,111,114,101,32,
54,46,48,46,48,34,62,10,32,32,32,60,114,100,102,58,82,68,70,32,120,109,108,110,115,58,114,100,102,61,34,104,116,116,112,58,47,47,119,119,119,46,119,51,46,111,114,103,47,49,57,57,57,47,48,50,47,50,50,45,
114,100,102,45,115,121,110,116,97,120,45,110,115,35,34,62,10,32,32,32,32,32,32,60,114,100,102,58,68,101,115,99,114,105,112,116,105,111,110,32,114,100,102,58,97,98,111,117,116,61,34,34,10,32,32,32,32,32,
32,32,32,32,32,32,32,120,109,108,110,115,58,116,105,102,102,61,34,104,116,116,112,58,47,47,110,115,46,97,100,111,98,101,46,99,111,109,47,116,105,102,102,47,49,46,48,47,34,10,32,32,32,32,32,32,32,32,32,
32,32,32,120,109,108,110,115,58,101,120,105,102,61,34,104,116,116,112,58,47,47,110,115,46,97,100,111,98,101,46,99,111,109,47,101,120,105,102,47,49,46,48,47,34,62,10,32,32,32,32,32,32,32,32,32,60,116,105,
102,102,58,79,114,105,101,110,116,97,116,105,111,110,62,49,60,47,116,105,102,102,58,79,114,105,101,110,116,97,116,105,111,110,62,10,32,32,32,32,32,32,32,32,32,60,101,120,105,102,58,80,105,120,101,108,
88,68,105,109,101,110,115,105,111,110,62,49,52,60,47,101,120,105,102,58,80,105,120,101,108,88,68,105,109,101,110,115,105,111,110,62,10,32,32,32,32,32,32,32,32,32,60,101,120,105,102,58,80,105,120,101,108,
89,68,105,109,101,110,115,105,111,110,62,49,49,55,60,47,101,120,105,102,58,80,105,120,101,108,89,68,105,109,101,110,115,105,111,110,62,10,32,32,32,32,32,32,60,47,114,100,102,58,68,101,115,99,114,105,112,
116,105,111,110,62,10,32,32,32,60,47,114,100,102,58,82,68,70,62,10,60,47,120,58,120,109,112,109,101,116,97,62,10,67,81,5,85,0,0,0,225,73,68,65,84,88,9,237,146,81,10,196,32,12,5,221,181,246,195,27,120,
10,239,127,12,79,180,11,133,129,34,26,226,103,72,10,75,186,149,7,102,230,125,106,173,191,214,90,42,165,60,191,251,190,159,153,115,78,210,243,149,14,165,179,8,10,116,2,78,192,17,8,8,71,134,154,115,9,107,
164,222,251,246,88,12,142,49,182,65,67,112,60,92,85,244,184,146,72,41,142,131,148,194,3,85,15,59,170,11,64,99,104,147,58,72,99,8,122,160,106,104,71,181,71,252,81,132,227,32,69,48,4,199,195,85,183,30,17,
141,248,121,110,131,136,158,3,252,247,64,213,208,142,91,143,248,154,39,197,56,14,82,12,67,112,60,92,85,237,17,241,20,66,29,68,60,65,15,84,61,236,168,46,0,226,105,208,113,144,6,121,160,106,104,71,209,35,
178,145,255,158,98,16,217,239,0,239,134,224,196,85,145,182,152,1,103,1,133,79,1,7,18,139,105,8,206,31,87,134,29,190,235,100,39,112,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* ChannelComponent::channelScale_png = (const char*) resource_ChannelComponent_channelScale_png;
const int ChannelComponent::channelScale_pngSize = 3644;


//[EndFile] You can add extra defines here...
//[/EndFile]

