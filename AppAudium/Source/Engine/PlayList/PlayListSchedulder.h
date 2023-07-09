/*
  ==============================================================================

    PlayListSchedulder.h
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Engine/AudioRegion.h"
#include "Engine/PlayList/SampleTimer.h"

class TransportSourceProvider;
class PlayListContainer;
class PlayListItem;


class PlayListScheduler : public juce::AudioIODeviceCallback {
    
    
public:
    PlayListScheduler(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                      std::shared_ptr<TransportSourceProvider> transportSourceProvider,
                      std::shared_ptr<PlayListContainer> playListContainer);
    ~PlayListScheduler() override;
    
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int totalNumInputChannels,
                                           float* const* outputChannelData,
                                           int totalNumOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

    
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;

    void audioDeviceStopped() override;

    void prepareToPlay (double sampleRate, int blockSize);
    
    void start();
    void stop();
    void setPlayListItemIndex(int playListItemIndex);
    bool isPlaying() const noexcept { return sampleTimer.isActive(); }
    
private:
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<TransportSourceProvider> transportSourceProvider;
    std::shared_ptr<PlayListContainer> playListContainer;
    
    SampleTimer sampleTimer;

    double sampleRate = 0.0;
    int bufferSize = 0;
    
    std::atomic<int> playListItemIndex = 0;
    AudioRegion::RegionData currentRegionData;
    
    juce::CriticalSection readLock;
    
    int secondsToSamples(double seconds)
    {
        return static_cast<int>(seconds * sampleRate);
    }
    
    double samplesToSeconds(int samples)
    {
        jassert(sampleRate > 0.0);
        return static_cast<double>(samples) / sampleRate;
    }
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};
