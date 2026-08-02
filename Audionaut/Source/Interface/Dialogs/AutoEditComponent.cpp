//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#include "AutoEditComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListItem.h"

AutoEditComponent::AutoEditComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
    audiumEngine(audiumEngine_)
{

    // Selected audio track
    selectedTrack = std::make_unique<juce::ComboBox>();
    addAndMakeVisible (selectedTrack.get());

    selectedTrackLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Audio track:"));
    selectedTrackLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
    selectedTrackLabel->attachToComponent (selectedTrack.get(), true);
    
    // Selected playlist item
    selectedPlaylistItem = std::make_unique<juce::ComboBox>();
    addAndMakeVisible (selectedPlaylistItem.get());

    selectedPlaylistItemLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Audio clip:"));
    selectedPlaylistItemLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
    selectedPlaylistItemLabel->attachToComponent (selectedPlaylistItem.get(), true);


    // Number of segments
    numSegments = std::make_unique<juce::Slider>("Num Segments Slider Font 13");
    addAndMakeVisible (numSegments.get());
    AudiumLookAndFeel::configureSlider(numSegments.get());
    numSegments->setColour (juce::Slider::backgroundColourId, juce::Colours::black);
    numSegments->setVelocityModeParameters(1.0, 1, 0.01);
    numSegments->setNormalisableRange(juce::NormalisableRange<double>(0, 16000, 1));
    numSegments->onValueChange = [this]() {
    };
    numSegments->setTextValueSuffix (" segments");
    numSegments->setValue(20.0, juce::dontSendNotification);
    numSegments->updateText();
    
    numSegmentsLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Cut up into number of:"));
    numSegmentsLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
    numSegmentsLabel->attachToComponent (numSegments.get(), true);


    // minimum length
    segmentMin = std::make_unique<juce::Slider>("Min Segments Slider Font 13");
    addAndMakeVisible (segmentMin.get());
    AudiumLookAndFeel::configureSlider(segmentMin.get());
    segmentMin->setColour (juce::Slider::backgroundColourId, juce::Colours::black);
    segmentMin->setVelocityModeParameters(1.0, 1, 0.1);
    segmentMin->setTextValueSuffix (" seconds");
    segmentMin->setNormalisableRange(juce::NormalisableRange<double>(0.1, 60.0, 1));
    segmentMin->onValueChange = [this]() {
    };
    segmentMin->setValue(2.0, juce::dontSendNotification);
    segmentMin->updateText();
    
    segmentMinLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Minimum segment length:"));
    segmentMinLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
    segmentMinLabel->attachToComponent (segmentMin.get(), true);
    

    // maximum length
    segmentMax = std::make_unique<juce::Slider>("Max Segments Slider Font 13");
    addAndMakeVisible (segmentMax.get());
    AudiumLookAndFeel::configureSlider(segmentMax.get());
    segmentMax->setColour (juce::Slider::backgroundColourId, juce::Colours::black);
    segmentMax->setVelocityModeParameters(1.0, 1, 0.01);
    segmentMax->setTextValueSuffix (" seconds");
    segmentMax->setNormalisableRange(juce::NormalisableRange<double>(0, 600, 1));
    segmentMax->onValueChange = [this]() {
    };
    segmentMax->setValue(60.0, juce::dontSendNotification);
    segmentMax->updateText();
    
    segmentMaxLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Maximum segment length:"));
    segmentMaxLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
    segmentMaxLabel->attachToComponent (segmentMax.get(), true);

    setSize (500, 250);
    
    
    selectedTrack->addListener(this);
}

void AutoEditComponent::resized()
{
    juce::Rectangle<int> r(proportionOfWidth(0.5f), 20, proportionOfWidth(0.4f), 3000);
    
    const int h = 23;
    const int space = h / 4;
    
    if (selectedTrack != nullptr) {
        selectedTrack->setBounds (r.removeFromTop (h));
        r.removeFromTop (space);
    }
    
    if (selectedPlaylistItem != nullptr) {
        selectedPlaylistItem->setBounds (r.removeFromTop (h));
        r.removeFromTop (space);
    }
    
    if (numSegments != nullptr) {
        numSegments->setBounds (r.removeFromTop (h));
        r.removeFromTop (space);
    }
    
    if (segmentMin != nullptr) {
        segmentMin->setBounds (r.removeFromTop (h));
        r.removeFromTop (space);
    }
    
    if (segmentMax != nullptr) {
        segmentMax->setBounds (r.removeFromTop (h));
        r.removeFromTop (space);
    }
}

void AutoEditComponent::update()
{
    updateSelectedTrack();
    updateSelectedPlaylistItem();
}

const audium::PlayListItem* AutoEditComponent::getSelectedPlaylistItem() const
{
    auto selectedObjects = audiumEngine->getAudioTrackContainer()->getSelectionManager()->getSelectedObjects();
    for (auto object : selectedObjects) {
        if (auto playListItem = dynamic_cast<audium::PlayListItem*>(object.get())) {
            return playListItem;
        }
    }
    return nullptr;
}

void AutoEditComponent::updateSelectedTrack()
{
    selectedTrack->removeListener(this);
    
    selectedTrack->clear();
    
    for (auto track : audiumEngine->getAudioTrackContainer()->getAudioTracks()) {
        selectedTrack->addItem(track->getAudioTrackName(), track->getId() + 1);
    }
    
    if (selectedTrack->getNumItems() > 0) {
        
        if (auto selectedItem = getSelectedPlaylistItem()) {
            auto trackId = selectedItem->getRegion()->getAudioTrack()->getId();
            selectedTrack->setSelectedId(trackId + 1, juce::dontSendNotification);
        }
        else {
            selectedTrack->setSelectedId(1, juce::dontSendNotification);
        }
    }
    else {
        selectedTrack->setText("n/a", juce::dontSendNotification);
    }
    
    selectedTrack->addListener(this);
}

void  AutoEditComponent::updateSelectedPlaylistItem()
{
    
    
    selectedPlaylistItem->clear();
    auto minLength = 1.0;
    if (auto selectedItem = getSelectedPlaylistItem()) {
        auto track = selectedItem->getRegion()->getAudioTrack();
        for (auto item : track->getPlayListContainer()->getPlayListItems()) {
            if (item->getRegion()->getRegionData(audium::seconds).getLength() >= minLength) {
    
                selectedPlaylistItem->addItem(item->getRegion()->getName(), item->getId() + 1);
            }
        }
        selectedPlaylistItem->setSelectedId(selectedItem->getId() + 1, juce::dontSendNotification);
    }
    else {
        // fallback to default track
        if (auto track = audiumEngine->getAudioTrackContainer()->getDefaultGroup()) {
            for (auto item : track->getPlayListContainer()->getPlayListItems()) {
                if (item->getRegion()->getRegionData(audium::seconds).getLength() >= minLength) {
                    selectedPlaylistItem->addItem(item->getRegion()->getName(), item->getId() + 1);
                }
            }
        }
        
        if (selectedPlaylistItem->getNumItems() > 0) {
            selectedPlaylistItem->setSelectedId(1, juce::dontSendNotification);
        }
        else {
            selectedPlaylistItem->setText("n/a", juce::dontSendNotification);
        }
    }
}


void AutoEditComponent::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == selectedTrack.get()) {
        auto trackId = selectedTrack->getSelectedId() - 1;
        if (auto track = audiumEngine->getAudioTrackContainer()->getAudioTrack(trackId)) {
            track->getAudioTrackContainer().getSelectionManager()->deselectAll();
            track->setSelected(true);
            updateSelectedPlaylistItem();
        }
    }
}

audium::AutoEditConfig& AutoEditComponent::getAutoEditConfig()
{
    config.mode = "random";
    config.duration = 180;
    config.numSegments = static_cast<int>(numSegments->getValue());
    config.minSegLength = segmentMin->getValue();
    config.maxSegLength = segmentMax->getValue();
    config.trackId = selectedTrack->getSelectedId() - 1;
    config.playlistItemId = selectedPlaylistItem->getSelectedId() - 1;
    // config.source is left at its default: the dialog only ever runs the
    // built-in analysis. The Python path is reachable from tests, which is
    // where the two get compared.

    return config;
}
