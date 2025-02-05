/*
  ==============================================================================

    AudioBusRenderer.cpp
    Created: 9 Jan 2025 12:19:36pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioBusRenderer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/Playback.h"

namespace audium
{

template <class SampleType>
void AudioBusRenderer<SampleType>::setNumAudioBusChannels(int numChannels)
{
    if (numChannels != audioBus.getNumChannels()) {
        audioBus.setSize(numChannels, audioBus.getNumSamples());
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    audioBus.setSize(audioBus.getNumChannels(), samplesPerBlockExpected);
    
    channelBuffer.setSize(1, samplesPerBlockExpected);
    stereoBuffer.setSize(2, samplesPerBlockExpected);
    
    for (auto i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
        juce::dsp::ProcessSpec spec;
        spec.numChannels         = 2;
        spec.maximumBlockSize    = samplesPerBlockExpected;
        spec.sampleRate          = sampleRate;
        panners[i].prepare(spec);
        
        spec.numChannels        = 1;
        gains[i].setRampDurationSeconds(0.01);
        gains[i].prepare(spec);
        
        
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::processAudioBlock(const juce::AudioSourceChannelInfo& outputInfo)
{

    // number of output channels
    auto outputChannels = outputInfo.buffer->getNumChannels();
    
    // number of audio bus channels
    auto totalChannels = audioBus.getNumChannels();
    
    if (totalChannels > 0) {
        
        // render the entire bus (all channels)
        audioBus.clear();
        juce::AudioSourceChannelInfo busInfo (&audioBus, outputInfo.startSample, outputInfo.numSamples);
        playback->processAudioBlock(busInfo);
        
        for (auto i = 0; i < totalChannels; i++) {
            // copy channel buffer
            channelBuffer.clear();
            channelBuffer.copyFrom(0, busInfo.startSample, audioBus.getReadPointer(i), busInfo.numSamples);
            juce::dsp::AudioBlock<SampleType> in (channelBuffer);
            // process channel gain
            
            juce::dsp::ProcessContextReplacing<SampleType> gainContext(in);
            gains[i].process(gainContext);
            
            audioBus.copyFrom(i, busInfo.startSample, channelBuffer.getReadPointer(0), busInfo.numSamples);
            

            channelLevel[i].store(channelBuffer.getMagnitude(0, busInfo.startSample, busInfo.numSamples));
        }
        
        //
        
        // mono output
        if (outputChannels == 1) {
            for (auto i = 0; i < totalChannels; i++) {
                outputInfo.buffer->addFrom(0,
                                           busInfo.startSample,
                                           audioBus.getReadPointer(i),
                                           busInfo.numSamples);
            }
            
        } // stereo
        else if (outputChannels == 2) {
            for (auto i = 0; i < totalChannels; i++) {
                
                // copy channel buffer
                channelBuffer.clear();
                channelBuffer.copyFrom(0, busInfo.startSample, audioBus.getReadPointer(i), busInfo.numSamples);
                juce::dsp::AudioBlock<SampleType> in (channelBuffer);
                
                
                // clear the stereo buffer
                stereoBuffer.clear();
                juce::dsp::AudioBlock<SampleType> out (stereoBuffer);
                
                // process stereo pan
                juce::dsp::ProcessContextNonReplacing<SampleType> panContext(in, out);
                panners[i].process(panContext);
                
                // mix output
                for (auto c = 0; c < outputChannels; c++) {
                    outputInfo.buffer->addFrom(c,
                                               outputInfo.startSample,
                                               stereoBuffer.getReadPointer(c),
                                               outputInfo.numSamples);
                }
            }
            
            // master gain
            juce::dsp::AudioBlock<SampleType> master (*outputInfo.buffer);
            juce::dsp::ProcessContextReplacing<SampleType> gainContext(master);
            masterGain.process(gainContext);
            
            // master level
            for (auto m = 0; m < outputChannels; ++m) {
                masterLevel[m].store(outputInfo.buffer->getMagnitude(m, outputInfo.startSample, outputInfo.numSamples));
            }
        }
        else // multichannel output
        {
            jassert(outputChannels == totalChannels);
            
            for (auto c = 0; c < std::min(outputChannels, totalChannels); c++) {
                outputInfo.buffer->addFrom(c,
                                           busInfo.startSample,
                                           audioBus.getReadPointer(c),
                                           busInfo.numSamples);
            }
        }
    }
}

// TODO: only float supported at this time
template class AudioBusRenderer<float>;

// TODO: juce::AudioSourceChannelInfo must support double
//template class AudioBusRenderer<double>;

} // namespace audium
