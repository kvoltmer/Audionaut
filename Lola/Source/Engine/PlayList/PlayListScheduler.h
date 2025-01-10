/*
  ==============================================================================

    PlayListScheduler.h
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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

class PlayListContainer;
class PlayListItem;
class TransportSourceContainer;
class AudioResourceContainer;

namespace audium {
    class Playback;
    class LockFreeCommander;
    
template <class>
    class AudioBusRenderer;
}

class PlayListScheduler : public juce::ChangeListener
{
    
    
public:
    PlayListScheduler(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                      std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                      std::shared_ptr<TempoProvider> tempoProvider_,
                      std::shared_ptr<audium::LinkEngine> linkEngine_,
                      std::shared_ptr<AudioClipContainer> audioClipContainer_,
                      std::shared_ptr<TransportSourceContainer> transportSourceContainer_,
                      std::shared_ptr<audium::Playback> playback_,
                      std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer_,
                      std::shared_ptr<audium::LockFreeCommander> lockFreeCommander_) :
        audioTrackContainer(audioTrackContainer_),
        audioResourceContainer(audioResourceContainer_),
        tempoProvider(tempoProvider_),
        linkEngine(linkEngine_),
        audioClipContainer(audioClipContainer_),
        transportSourceContainer(transportSourceContainer_),
        playback(playback_),
        audioBusRenderer(audioBusRenderer_),
        lockFreeCommander(lockFreeCommander_)
    {
        linkEngine->tickCallback = [this](bool isPlaying, double beats, int numSamples) { tick(isPlaying, beats, numSamples); };
        
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
    double getAbsoluteStartPosition(audium::TimeContextType context) const;

    void tick(bool isPlaying, double beats, int numSamples);
    
    void processAudio (const juce::AudioSourceChannelInfo& info);
    
    double getTotalLength(audium::TimeContextType context, bool addOverhead = false) const;
    
    void bounceToFile(juce::AudioFormatWriter* writer,
                      audium::ExportAudioConfig &config,
                      std::function<void ()> callback);
    
    audium::LinkEngine* getLinkEngine() const { return linkEngine.get(); }
    
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    
    // TODO: maybe move this to class PositionableBase
    static juce::Range<double> absoluteToLocalRange(juce::Range<double> absoluteRange, const PlayListItem* item, audium::TimeContextType context);
    static double absoluteToLocalPosition(double absolutePosition, const PlayListItem* item, audium::TimeContextType context);
    static juce::Range<double> absoluteToLocalRange(juce::Range<double> absoluteRange, std::shared_ptr<AudioSubGroup> subGroup, audium::TimeContextType context);
    
    void commitPlayListData();
    
    PlayListSchedulerData data;
    


    
private:
    
    // process sequencing
    void process(double absolutePosition, int numSamples);
    
private:
    
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<audium::LinkEngine> linkEngine;
    std::shared_ptr<AudioClipContainer> audioClipContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::shared_ptr<audium::Playback> playback;
    std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer;
    std::shared_ptr<audium::LockFreeCommander> lockFreeCommander;
    
    double externalSampleRate = 0.0;
    
    int bufferSize = 0;
    
    std::atomic<bool> forcePosition = false;
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};
