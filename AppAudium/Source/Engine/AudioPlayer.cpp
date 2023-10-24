/*
  ==============================================================================

    AudioPlayer.cpp
    Created: 23 Mar 2023 11:13:52am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioPlayer.h"
#include "AudiumTransportSource.h"

AudioPlayer::AudioPlayer(std::shared_ptr<AudiumTransportSource> audioTransportSource,
                         std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                         juce::InputSource* inputSource,
                         juce::AudioFormatManager& formatManager,
                         juce::TimeSliceThread* readAheadThread) :
    audioTransportSource(audioTransportSource),
    audioDeviceManager(audioDeviceManager)
{
    audioDeviceManager->addAudioCallback(this);
    setSource (audioTransportSource.get());
    

    auto stream = rawToUniquePtr (inputSource->createInputStream());
    jassert(stream != nullptr);
    if (stream == nullptr)
        return;

    auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream)));
    jassert(reader);
    if (reader == nullptr)
        return;

    audioFormatReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);

    sampleRate = audioFormatReaderSource->getAudioFormatReader()->sampleRate;
    numChannels = audioFormatReaderSource->getAudioFormatReader()->numChannels;
    // plug it into the transport source
    audioTransportSource->setSource (audioFormatReaderSource.get(),
                                    32768,                   // tells it to buffer this many samples ahead
                                    readAheadThread,                 // this is the background thread to use for reading-ahead
                                    sampleRate);     // allows for sample rate correction

    
    
}


AudioPlayer::~AudioPlayer()
{
    audioTransportSource->setSource (nullptr);
    setSource (nullptr);
    
    audioDeviceManager->removeAudioCallback(this);
}

/** Creates a buffer using a pre-allocated block of memory.

    Note that if the buffer is resized or its number of channels is changed, it
    will re-allocate memory internally and copy the existing data to this new area,
    so it will then stop directly addressing this memory.

    @param dataToReferTo    a pre-allocated array containing pointers to the data
                            for each channel that should be used by this buffer. The
                            buffer will only refer to this memory, it won't try to delete
                            it when the buffer is deleted or resized.
    @param numChannelsToUse the number of channels to use - this must correspond to the
                            number of elements in the array passed in
    @param numSamples       the number of samples to use - this must correspond to the
                            size of the arrays passed in
*/
//AudioBuffer (Type* const* dataToReferTo,
//             int numChannelsToUse,
//             int numSamples)
//    : numChannels (numChannelsToUse),
//      size (numSamples)
//{
//    jassert (dataToReferTo != nullptr);
//    jassert (numChannelsToUse >= 0 && numSamples >= 0);
//    allocateChannels (dataToReferTo, 0);
//}

void AudioPlayer::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                       int totalNumInputChannels,
                                       float* const* outputChannelData,
                                       int totalNumOutputChannels,
                                       int numSamples,
                                       const juce::AudioIODeviceCallbackContext& context)
{
    if (not byPass)
    {
        juce::AudioSourcePlayer::audioDeviceIOCallbackWithContext(inputChannelData, totalNumInputChannels, outputChannelData, totalNumOutputChannels, numSamples, context);
    }
    
    // workaroud to get the output level...
    juce::AudioBuffer<float> temp(outputChannelData, totalNumOutputChannels, numSamples);
    outputLevel = temp.getMagnitude(0, 0, temp.getNumSamples());
}

void AudioPlayer::renderOffline(float* const* outputChannelData, int totalNumOutputChannels, int numSamples)
{
    
    juce::AudioIODeviceCallbackContext context;
    juce::AudioSourcePlayer::audioDeviceIOCallbackWithContext(nullptr, 0, outputChannelData, totalNumOutputChannels, numSamples, context);
    
}

float AudioPlayer::getOutputLevel() const
{
    return outputLevel.load();
}
