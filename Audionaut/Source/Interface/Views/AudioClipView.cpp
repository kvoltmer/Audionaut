//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "AudioClipView.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Engine/Resource/AudioResource.h"
#include "Interface/ColourIds.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Analysis/AnalysisProvider.h"

using namespace juce;

double AudioClipView::getRegionStart(audium::TimeContextType context) const
{
    return playListItem->getRegion()->getRegionData(audium::seconds).getStart();
}

void AudioClipView::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider();

    if (analysisProvider != nullptr && source == analysisProvider.get())
    {
        refreshSegments();
        return;
    }

    // Not our analysis provider - fall back to the base class (thumbnail change).
    WaveFormViewBase::changeListenerCallback(source);
}

double AudioClipView::getClipGain() const
{
    return playListItem->getDynamics().getGain(channelNumber);
}

void AudioClipView::resized()
{
    fadeInOutView->setBounds(getLocalBounds());
    segmentationView->setBounds(getLocalBounds());

    auto sliderWidth = 67;
    auto sliderHeight = 15;
    auto space = 5;
    
    if (getWidth() > sliderWidth + (space * 2)) {
        volumeSlider->setVisible(true);
        volumeSlider->setBounds (space,
                                 getHeight() - sliderHeight - space,
                                 sliderWidth,
                                 sliderHeight);
    }
    else {
        volumeSlider->setVisible(false);
    }
}

void AudioClipView::updateUI(std::shared_ptr<audium::AudioResource> audioResource_, int theChannel)
{
    //std::cout << "AudioClipView::updateUI " << playListItem->getRegion()->getName() << std::endl;
    channelNumber = theChannel;
    if (audioResource != audioResource_) {
        audioResource = audioResource_;
        createThumbnailCache();
    }
    else {
        //std::cout << "AudioClipView::updateUI " << this << " null" << std::endl;
    }
        
    volumeSlider->setValue(LevelMeter::gainToDecebel(getClipGain()), dontSendNotification);

    refreshSegments();
}

void AudioClipView::refreshSegments()
{
    if (playListItem == nullptr || audioResource == nullptr)
        return;

    auto audioTrack = playListItem->getRegion()->getAudioTrack();
    if (audioTrack == nullptr)
        return;

    auto analysisProvider = audioTrack->getAnalysisProvider();
    if (analysisProvider == nullptr)
        return;

    if (audioResource->isRecording())
        return;
    
    const auto audioFile = juce::File(audioResource->getFullPathName());

    // Only display the analysis types the track is configured to show.
    constexpr audium::AnalysisType allTypes[] = { audium::AnalysisType::SBic,
                                                  audium::AnalysisType::Onset,
                                                  audium::AnalysisType::Beat,
                                                  audium::AnalysisType::BeatDegara };

    int visibleTypes = 0;
    for (int i = 0; i < 4; ++i)
        if (audioTrack->getViewState().isAnalysisTypeVisible(allTypes[i]))
            visibleTypes |= (1 << i);

    // The queried results depend only on these inputs, so an unchanged set
    // means the overlay is already current - this runs on every clip layout
    // pass, and the cache lookups (mutex + vector copy per type) are too
    // expensive to repeat per zoom step or scroll notch.
    const auto generation = analysisProvider->getCache()->getGeneration();
    const auto regionStart = getRegionStart(audium::seconds);

    if (generation == lastAnalysisGeneration
        && visibleTypes == lastVisibleTypesMask
        && regionStart == lastRegionStartSeconds
        && audioFile.getFullPathName() == lastAnalysisPath)
        return;

    lastAnalysisGeneration = generation;
    lastVisibleTypesMask = visibleTypes;
    lastRegionStartSeconds = regionStart;
    lastAnalysisPath = audioFile.getFullPathName();

    std::unordered_map<audium::AnalysisType, std::vector<float>> segmentsByType;
    for (int i = 0; i < 4; ++i)
        if ((visibleTypes & (1 << i)) != 0)
            segmentsByType[allTypes[i]] = analysisProvider->getSegments(allTypes[i], audioFile);

    segmentationView->setSegments(std::move(segmentsByType), regionStart);
}

void AudioClipView::setPlayListItem(std::shared_ptr<audium::PlayListItem> item, bool volumeControlVisible)
{
    playListItem = item;

    fadeInOutView->setPlayListItem(item);

    refreshSegments();

    volumeSlider->onValueChange = [this] {
        playListItem->getDynamics().setGain(channelNumber, Decibels::decibelsToGain(volumeSlider->getValue()), true);
        this->audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateArrangementAction);
    };
    volumeSlider->onDragStart = [this] {
        playListItem->onDragStart();
    };
    
    volumeSlider->onDragEnd = [this] {
        playListItem->onDragEnd();
    };
    
    volumeSlider->setVisible(volumeControlVisible);
}


