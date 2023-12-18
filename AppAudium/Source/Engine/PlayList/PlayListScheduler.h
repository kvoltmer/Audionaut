/*
  ==============================================================================

    PlayListScheduler.h
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Engine/AudioRegion.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Link/LinkEngine.hpp"

class TransportSourceContainer;
class PlayListContainer;
class PlayListItem;
class AudioGroupContainer;
class AudioResourceContainer;

class PlayListScheduler
{
    
    
public:
    PlayListScheduler(std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                      std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                      std::shared_ptr<TempoProvider> tempoProvider,
                      std::shared_ptr<audium::LinkEngine> linkEngine) :
        audioGroupContainer(audioGroupContainer),
        audioResourceContainer(audioResourceContainer),
        tempoProvider(tempoProvider),
        linkEngine(linkEngine)
    {
        linkEngine->tickCallback = [this](bool isPlaying, double beats, int numSamples) { tick(isPlaying, beats, numSamples); };
        
    }
    
    ~PlayListScheduler() = default;

    void prepareToPlay (double sampleRate, int blockSize);
    

    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    void setFollowTransport(bool enable) { followTransport = enable; }
    bool getFollowTransport() const { return followTransport; }
    void setLoopPlayList(bool enable) { loopPlayList.store(enable); }
    bool getLoopPlayList() const { return loopPlayList.load(); }
    void setEditMode(bool bEditMode) { editMode = bEditMode; }
    bool isEditMode() const { return editMode; }
    bool isArrangementMode() const { return !editMode; }
    
    void resetCurrentPlayListItem();
    
    void setCurrentPositionAtPlayListItemIndex(std::shared_ptr<AudioGroup> group, int playListItemIndex);
    int getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioGroup> group) const;
    double getPlayListItemProgress(std::shared_ptr<AudioGroup> group, int playListItemIndex) const;
    
    double getAbsolutePositionClocks() const;
    
    double getAbsolutePositionSeconds() const;
    void setAbsolutePositionSeconds(double newPosition);
    
    void tick(bool isPlaying, double beats, int numSamples);
    
    void audioCallback (const juce::AudioSourceChannelInfo& info);
    
    double getTotalLengthClocks() const;
    double getTotalLengthSeconds() const;
    
    void bounceToFile(juce::AudioFormatWriter* writer, double sampleRate, int numSamples, int numOutputChannels);
    
    audium::LinkEngine* getLinkEngine() const { return linkEngine.get(); }
    
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    
private:

    double absoluteToLocalPosition(double absolutePosition, const PlayListItem* item) const;
    
    // Arrangement mode sequencing
    void processInArrangementMode(double pos, int numSamples);
    
    // Edit mode sequencing
    void processInEditMode(double absolutePosition, int numSamples);
    
private:
    
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<audium::LinkEngine> linkEngine;
    
    double sampleRate = 0.0;
    
    int bufferSize = 0;
    
    // transport position in 96th clocks
    double transportPositionClocks = 0.0;
    
    double startPositionClocks = 0.0;
    
    std::atomic<bool> forcePosition = false;
    
    juce::CriticalSection readLock;
    
    bool followTransport = true;
    
    std::atomic<bool> loopPlayList = false;
    
    // Edit or Arrangement Mode
    std::atomic<bool> editMode = true;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};
