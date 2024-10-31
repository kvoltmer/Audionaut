/*
  ==============================================================================

    PlayListScheduler.h
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/PlayList/PlayListSchedulerData.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/ExportAudioConfig.h"

class TransportSourceContainer;
class PlayListContainer;
class PlayListItem;

class AudioResourceContainer;

class PlayListScheduler : public juce::ChangeListener
{
    
    
public:
    PlayListScheduler(std::shared_ptr<AudioTrackContainer> audioTrackContainer,
                      std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                      std::shared_ptr<TempoProvider> tempoProvider,
                      std::shared_ptr<audium::LinkEngine> linkEngine,
                      std::shared_ptr<AudioClipContainer> audioClipContainer,
                      std::shared_ptr<TransportSourceContainer> transportSourceContainer) :
        audioTrackContainer(audioTrackContainer),
        audioResourceContainer(audioResourceContainer),
        tempoProvider(tempoProvider),
        linkEngine(linkEngine),
        audioClipContainer(audioClipContainer),
        transportSourceContainer(transportSourceContainer)
    {
        linkEngine->tickCallback = [this](bool isPlaying, double beats, int numSamples) { tick(isPlaying, beats, numSamples); };
        
        audioTrackContainer->addChangeListener(this);
    }
    
    ~PlayListScheduler()
    {
        audioTrackContainer->removeChangeListener(this);
    }
    
    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        commitPlayListData();
    }

    void prepareToPlay (double sampleRate, int blockSize);
    

    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    void setFollowTransport(bool enable) { data.followTransport = enable; }
    bool getFollowTransport() const { return data.followTransport; }
    void setLoopPlayList(bool enable) { data.loopPlayList = enable; }
    bool getLoopPlayList() const { return data.loopPlayList; }
    void setEditMode(bool bEditMode) { data.editMode = bEditMode; }
    bool isEditMode() const { return data.editMode; }
    bool isArrangementMode() const { return !data.editMode; }
        
    void setCurrentPositionAtPlayListItemIndex(std::shared_ptr<AudioTrack> track, int playListItemIndex);
    int getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioTrack> track);
    double getPlayListItemProgress(std::shared_ptr<AudioTrack> track, int playListItemIndex) const;
    
    
    double getAbsolutePosition(audium::TimeContextType context) const;
    void setAbsolutePosition(double newPosition, audium::TimeContextType context);
    
    void tick(bool isPlaying, double beats, int numSamples);
    
    void audioCallback (const juce::AudioSourceChannelInfo& info);
    
    double getTotalLength(audium::TimeContextType context, bool addOverhead = false) const;
    
    void bounceToFile(juce::AudioFormatWriter* writer,
                      audium::ExportAudioConfig &config,
                      std::function<void ()> callback);
    
    audium::LinkEngine* getLinkEngine() const { return linkEngine.get(); }
    
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    
    void commitPlayListData();
    
    PlayListSchedulerData data;
    
private:

    double absoluteToLocalPosition(double absolutePosition, const PlayListItem* item, audium::TimeContextType context) const;
    
    // process sequencing
    void process(double absolutePosition, int numSamples);
    
private:
    
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<audium::LinkEngine> linkEngine;
    std::shared_ptr<AudioClipContainer> audioClipContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    
    double externalSampleRate = 0.0;
    
    int bufferSize = 0;
    
    std::atomic<bool> forcePosition = false;
    
    std::atomic<bool> newDataCommited = false;
    
    juce::CriticalSection readLock;

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};
