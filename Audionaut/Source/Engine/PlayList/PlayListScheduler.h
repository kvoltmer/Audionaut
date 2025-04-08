//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>
#include <farbot/fifo.hpp>

#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/PlayList/PlayListSchedulerData.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Export/ExportAudioConfig.h"

namespace audium {

class PlayListContainer;
class PlayListItem;
class TransportSourceContainer;
class AudioResourceContainer;
class Playback;
class AudioBusInterface;


class PlayListScheduler : public juce::ChangeListener
{
    
    
public:
    PlayListScheduler(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                      std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                      std::shared_ptr<TempoProvider> tempoProvider_,
                      std::shared_ptr<audium::LinkEngine> linkEngine_,
                      std::shared_ptr<audium::AudioClipContainer> audioClipContainer_,
                      std::shared_ptr<TransportSourceContainer> transportSourceContainer_,
                      std::shared_ptr<audium::Playback> playback_,
                      std::shared_ptr<AudioBusInterface> audioBusInterface_,
                      std::shared_ptr<TransportLoop> transportLoop_) :
    audioTrackContainer(audioTrackContainer_),
    audioResourceContainer(audioResourceContainer_),
    tempoProvider(tempoProvider_),
    linkEngine(linkEngine_),
    audioClipContainer(audioClipContainer_),
    transportSourceContainer(transportSourceContainer_),
    playback(playback_),
    audioBusInterface(audioBusInterface_),
    transportLoop(transportLoop_)
    {
        linkEngine->tickCallback = [this](bool isPlaying, double beats, int numSamples) {
            tick(isPlaying, beats, numSamples);
        };
        
        audioTrackContainer->addChangeListener(this);
    }
    
    ~PlayListScheduler() override
    {
        audioTrackContainer->removeChangeListener(this);
    }
    
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        commitPlayListData();
    }
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    
    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    void setFollowTransport(bool enable) { data.followTransport = enable; }
    bool getFollowTransport() const { return data.followTransport; }
    void setEditMode(bool bEditMode) { data.editMode = bEditMode; }
    bool isEditMode() const { return data.editMode; }
    bool isArrangementMode() const { return !data.editMode; }
    
    void setCurrentPositionAtPlayListItemIndex(std::shared_ptr<AudioTrack> track, int playListItemIndex);
    int getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioTrack> track);
    double getPlayListItemProgress(std::shared_ptr<AudioTrack> track, int playListItemIndex) const;
    
    void setAbsoluteStartPosition(double newPosition, audium::TimeContextType context);
    double getAbsoluteStartPosition(audium::TimeContextType context) const;
    double getAbsolutePosition(audium::TimeContextType context) const;
    
    
    void tick(bool isPlaying, double beats, int numSamples);
    
    void processAudio (const juce::AudioSourceChannelInfo& info);
    
    double getTotalLength(audium::TimeContextType context, bool addOverhead = false) const;
    
    void bounceToFile(juce::AudioFormatWriter* writer,
                      audium::ExportAudioConfig &config,
                      std::function<void ()> callback);
    
    std::shared_ptr<audium::LinkEngine> getLinkEngine() const { return linkEngine; }
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    std::shared_ptr<audium::Playback> getPlayback() const { return playback; }
    std::shared_ptr<AudioBusInterface> getAudioBusInterface() const { return audioBusInterface; }
    std::shared_ptr<TransportLoop> getTransportLoop() const { return transportLoop; }
    
    void commitPlayListData();
    
    PlayListSchedulerData data;
    
private:
    
    // process sequencing
    void process(double absolutePosition, int numSamples, bool onLoop);
    
    
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<audium::LinkEngine> linkEngine;
    std::shared_ptr<audium::AudioClipContainer> audioClipContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::shared_ptr<audium::Playback> playback;
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    std::shared_ptr<TransportLoop> transportLoop;
    
    double externalSampleRate = 0.0;
    
    int bufferSize = 0;
    
    std::atomic<bool> forcePosition = false;
    
    std::atomic<double> totalLengthClocks = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};

} // namespace audium
