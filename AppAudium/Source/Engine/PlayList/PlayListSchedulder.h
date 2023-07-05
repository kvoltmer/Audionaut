/*
  ==============================================================================

    PlayListSchedulder.h
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class PlayListScheduler : public juce::AudioIODeviceCallback {
    
    
public:
    PlayListScheduler(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager);
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
    
private:
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    
    double sampleRate = 0.0;
    int bufferSize = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};
