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
    bool isPlaying() const noexcept { return playing; }
    
    void setPlayListItemIndex(int playListItemIndex);
    int getPlayListItemIndex() const;
    double getPlayListItemProgress(int playListItemIndex) const;
    
    double getAbsolutePosition() const;
    void setAbsolutePosition(double newPosition);
    
private:

    void tick(int numSamples);

    double absoluteToLocalPosition(double absolutePosition, std::shared_ptr<PlayListItem> item) const;
    void applyAbsolutePosition(double pos, bool shouldStart);
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<TransportSourceProvider> transportSourceProvider;
    std::shared_ptr<PlayListContainer> playListContainer;
    
    std::shared_ptr<PlayListItem> currentPlayListItem;

    double sampleRate = 0.0;
    int bufferSize = 0;
    
    std::atomic<uint64_t> transportPositionSamples = 0;
    std::atomic<double> transportPositionSeconds = 0.0;
    
    std::atomic<bool> playing { false };
    
    
    juce::CriticalSection readLock;
    
    uint64_t secondsToSamples(double seconds)
    {
        return static_cast<uint64_t>(seconds * sampleRate);
    }
    
    double samplesToSeconds(uint64_t samples)
    {
        jassert(sampleRate > 0.0);
        return static_cast<double>(samples) / sampleRate;
    }
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};
