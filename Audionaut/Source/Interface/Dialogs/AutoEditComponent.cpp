//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#include "AutoEditComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListItem.h"

AutoEditComponent::AutoEditComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
    audiumEngine(audiumEngine_)
{
    messageLabel = std::make_unique<juce::Label> (juce::String{},
                                                  TRANS ("Analyze and automatically edit the selected audio clip."));
    messageLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
    addAndMakeVisible (messageLabel.get());

    // Replace the clip with its segments
    replacePlayListItem = std::make_unique<juce::ToggleButton>(TRANS ("Replace clip with segments"));
    addAndMakeVisible (replacePlayListItem.get());
    replacePlayListItem->setToggleState (true, juce::dontSendNotification);

    // The abstract parameter: segment length in musical measures. It drives
    // the concrete numSegments slider below; 0 turns it off.
    segmentMeasures = std::make_unique<juce::Slider>("Segment Measures Slider Font 13");
    addAndMakeVisible (segmentMeasures.get());
    AudiumLookAndFeel::configureSlider(segmentMeasures.get());
    segmentMeasures->setColour (juce::Slider::backgroundColourId, juce::Colours::black);
    segmentMeasures->setVelocityModeParameters(1.0, 1, 0.01);
    segmentMeasures->setNormalisableRange(juce::NormalisableRange<double>(0, 64, 1));
    segmentMeasures->onValueChange = [this]() {
        updateDerivedParameters();
        updatePreview();
    };
    segmentMeasures->setTextValueSuffix (" measures");
    segmentMeasures->setValue(4.0, juce::dontSendNotification);
    segmentMeasures->updateText();

    segmentMeasuresLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Cut into segments of:"));
    segmentMeasuresLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
    segmentMeasuresLabel->attachToComponent (segmentMeasures.get(), true);


    // Number of segments
    numSegments = std::make_unique<juce::Slider>("Num Segments Slider Font 13");
    addAndMakeVisible (numSegments.get());
    AudiumLookAndFeel::configureSlider(numSegments.get());
    numSegments->setColour (juce::Slider::backgroundColourId, juce::Colours::black);
    numSegments->setVelocityModeParameters(1.0, 1, 0.01);
    numSegments->setNormalisableRange(juce::NormalisableRange<double>(0, 16000, 1));
    numSegments->onValueChange = [this]() {
        // Direct control over the concrete count: the abstract measure
        // parameter lets go so the two never fight over it.
        segmentMeasures->setValue(0.0, juce::dontSendNotification);
        segmentMeasures->updateText();
        updatePreview();
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
        // Direct control, so the abstract measure parameter lets go.
        segmentMeasures->setValue(0.0, juce::dontSendNotification);
        segmentMeasures->updateText();
        updatePreview();
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
        // Direct control, so the abstract measure parameter lets go.
        segmentMeasures->setValue(0.0, juce::dontSendNotification);
        segmentMeasures->updateText();
        updatePreview();
    };
    segmentMax->setValue(60.0, juce::dontSendNotification);
    segmentMax->updateText();

    segmentMaxLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Maximum segment length:"));
    segmentMaxLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
    segmentMaxLabel->attachToComponent (segmentMax.get(), true);

    applyButton = std::make_unique<juce::TextButton> (TRANS ("Apply"));
    addAndMakeVisible (applyButton.get());
    applyButton->addShortcut (juce::KeyPress (juce::KeyPress::returnKey));
    applyButton->onClick = [this]() {
        if (onApply)
            onApply();
    };

    setSize (500, 240);

    audiumEngine->getAudioTrackContainer()->getSelectionManager()->addChangeListener(this);
}

AutoEditComponent::~AutoEditComponent()
{
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->removeChangeListener(this);

    // The preview only makes sense while the Auto Edit window is open.
    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->clearMergePreview();
}

void AutoEditComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    update();
}

void AutoEditComponent::resized()
{
    if (messageLabel != nullptr)
        messageLabel->setBounds (10, 10, getWidth() - 20, 24);

    juce::Rectangle<int> r(proportionOfWidth(0.5f), 50, proportionOfWidth(0.4f), 3000);

    const int h = 23;
    const int space = h / 4;

    if (segmentMeasures != nullptr) {
        segmentMeasures->setBounds (r.removeFromTop (h));
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

    if (replacePlayListItem != nullptr) {
        replacePlayListItem->setBounds (r.removeFromTop (h));
        r.removeFromTop (space);
    }

    if (applyButton != nullptr)
        applyButton->setBounds (getWidth() - 110, getHeight() - 40, 100, 28);
}

void AutoEditComponent::update()
{
    if (auto window = findParentComponentOfClass<juce::DialogWindow>())
        window->setName (getTitle());

    // The target may have changed, and with it the length and tempo the
    // measure parameter derives from.
    updateDerivedParameters();
    updatePreview();
}

void AutoEditComponent::updateDerivedParameters()
{
    if (segmentMeasures->getValue() <= 0.0)
        return;

    audium::AutoEdit autoEdit(audiumEngine);
    auto& config = getAutoEditConfig();

    const auto derived = autoEdit.resolveNumSegments(config);

    numSegments->setValue(derived, juce::dontSendNotification);
    numSegments->updateText();

    const auto bounds = autoEdit.resolveSegmentLengthBounds(config);

    if (! bounds.isEmpty())
    {
        segmentMin->setValue(bounds.getStart(), juce::dontSendNotification);
        segmentMin->updateText();

        segmentMax->setValue(bounds.getEnd(), juce::dontSendNotification);
        segmentMax->updateText();
    }
}

void AutoEditComponent::updatePreview()
{
    audium::AutoEdit autoEdit(audiumEngine);
    autoEdit.previewAutoEdit(getAutoEditConfig());
}

juce::String AutoEditComponent::getTitle() const
{
    juce::String title (TRANS ("Auto Edit"));

    if (auto item = getTargetPlaylistItem()) {
        title << " " << item->getRegion()->getAudioTrack()->getAudioTrackName()
              << " : " << item->getRegion()->getName();
    }

    return title;
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

const audium::PlayListItem* AutoEditComponent::getTargetPlaylistItem() const
{
    if (auto selectedItem = getSelectedPlaylistItem())
        return selectedItem;

    // fall back to the default (first selected or first) track
    auto minLength = 1.0;
    if (auto track = audiumEngine->getAudioTrackContainer()->getDefaultGroup()) {
        for (auto item : track->getPlayListContainer()->getPlayListItems()) {
            if (item->getRegion()->getRegionData(audium::seconds).getLength() >= minLength) {
                return item.get();
            }
        }
    }

    return nullptr;
}

audium::AutoEditConfig& AutoEditComponent::getAutoEditConfig()
{
    config.mode = "random";
    config.duration = 180;
    config.segmentMeasures = segmentMeasures->getValue();
    config.numSegments = static_cast<int>(numSegments->getValue());
    config.minSegLength = segmentMin->getValue();
    config.maxSegLength = segmentMax->getValue();

    config.trackId = -1;
    config.playlistItemId = -1;
    if (auto item = getTargetPlaylistItem()) {
        config.trackId = item->getRegion()->getAudioTrack()->getId();
        config.playlistItemId = item->getId();
    }

    config.replacePlayListItem = replacePlayListItem->getToggleState();
    // config.source is left at its default: the dialog only ever runs the
    // built-in analysis. The Python path is reachable from tests, which is
    // where the two get compared.

    return config;
}
