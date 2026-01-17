//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "PlaybackDefines.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/Playback.h"

namespace audium
{


/**
 * @class AudioBusRenderer
 * @brief Handles audio bus processing, including channel management, panning, gain, and playback.
 *
 * The `AudioBusRenderer` class is responsible for managing audio channels, applying panning and gain,
 * and processing audio blocks for playback. It integrates with the `Playback` system to ensure
 * efficient audio rendering.
 *
 * @tparam SampleType The type of audio sample (e.g., float or double).
 */
template <class SampleType>
class AudioBusRenderer {
    
    
public:
    AudioBusRenderer(std::shared_ptr<audium::Playback> playback_) :
        playback(playback_)
    {
        setMasterGain(1.f);
        reset();
    }
    
    ~AudioBusRenderer() = default;
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    void setNumAudioBusChannels(int numChannels);
        
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        auto& outputBlock = context.getOutputBlock();
        outputBlock.clear();
        auto outputChannels = outputBlock.getNumChannels();

        auto& inputBlock = context.getInputBlock();
        auto inputChannels = inputBlock.getNumChannels();
        
        juce::dsp::AudioBlock<SampleType> audioBusBlock(audioBus);
        auto totalChannels = audioBus.getNumChannels();
        
        auto numSamples = outputBlock.getNumSamples();
        
        if (totalChannels > 0) {
            
            // render the entire bus (all channels)
            audioBus.clear();
            juce::AudioSourceChannelInfo busInfo (&audioBus, 0, static_cast<int>(numSamples));
            playback->processAudioBlock(busInfo);
            
            // process gain for each audio bus channel
            for (auto i = 0; i < totalChannels; i++) {
                auto channelBlock = audioBusBlock.getSingleChannelBlock(i);
                juce::dsp::ProcessContextReplacing<SampleType> gainContext( channelBlock );
                gains[i].process(gainContext);
                channelLevel[i].store(audioBus.getMagnitude(i,
                                                            0,
                                                            static_cast<int>(numSamples)));
            }
            
            // mono output
            if (outputChannels == 1) {
                for (auto i = 0; i < totalChannels; i++) {
                    // add from audio bus to mono output
                    outputBlock.getSingleChannelBlock(0).add( audioBusBlock.getSingleChannelBlock(i) );
                }
                
            } // stereo
            else if (outputChannels == 2) {
                for (auto i = 0; i < totalChannels; i++) {

                    stereoBuffer.clear();
                    juce::dsp::AudioBlock<SampleType> stereoBlock (stereoBuffer);
                    auto channelBlock = audioBusBlock.getSingleChannelBlock(i);
                    
                    // process stereo pan
                    juce::dsp::ProcessContextNonReplacing<SampleType> panContext(channelBlock,
                                                                                 stereoBlock);
                    panners[i].process(panContext);
                    
                    // mix output
                    for (auto c = 0; c < outputChannels; c++) {
                        outputBlock.getSingleChannelBlock(c).add( stereoBlock.getSingleChannelBlock(c) );
                    }
                }
                
                // master gain
                juce::dsp::ProcessContextReplacing<SampleType> gainContext(outputBlock);
                masterGain.process(gainContext);
                
                // master level
                for (auto m = 0; m < outputChannels; ++m) {
                    auto minmax = outputBlock.getSingleChannelBlock(m).findMinAndMax();
                    masterLevel[m].store( minmax.getEnd() );
                }
            }
            else { // multichannel output
                jassert(outputChannels == totalChannels);
                for (auto c = 0; c < std::min((int)outputChannels, totalChannels); c++) {
                    outputBlock.getSingleChannelBlock(c).add( audioBusBlock.getSingleChannelBlock(c) );
                }
            }
        }
    }
    
    void setPan(const int channelNumber, const SampleType newPan)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            // std::cout << "setPan " << channelNumber << " " << newPan << std::endl;
            panners[channelNumber].setPan(newPan);
        }
    }

    void setGain(const int channelNumber, const SampleType newGain)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            // std::cout << "setGain " << channelNumber << " " << newGain << std::endl;
            if (soloStates[channelNumber])
                gains[channelNumber].setGainLinear(newGain);
            else if (!muteStates[channelNumber])
                gains[channelNumber].setGainLinear(newGain);
            gainStates[channelNumber] = newGain;
        }
    }
    
    void setMute(const int channelNumber, const bool bMute)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            //std::cout << "setMute " << channelNumber << " " << bMute << std::endl;
            muteStates[channelNumber] = bMute;
            if (!soloStates[channelNumber])
                gains[channelNumber].setGainLinear(bMute ? 0.f : gainStates[channelNumber]);
        }
    }

    void setSolo(const int channelNumber, const bool bSolo)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            //std::cout << "setSolo " << channelNumber << " " << bSolo << std::endl;
            soloStates[channelNumber] = bSolo;

            auto anySolo = false;
            for (auto c = 0; c < audioBus.getNumChannels(); ++c) {
                if (soloStates[c]) {
                    anySolo = true;
                    break;
                }
            }
            
            for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
                if (anySolo) {
                    if (soloStates[c])
                        gains[c].setGainLinear(gainStates[c]);
                    else
                        gains[c].setGainLinear(0.f);
                }
                else {
                    // mute stats apply
                    gains[c].setGainLinear(muteStates[c] ? 0.f : gainStates[c]);
                }
            }
        }
    }
    
    void setMasterGain(const float newGain)
    {
        masterGain.setGainLinear(newGain);
    }
    
    const float getChannelLevel(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            return channelLevel[channelNumber].load();
        
        return 0.f;
    }
    
    const float getMasterLevel(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < 2)
            return masterLevel[channelNumber].load();
        
        return 0.f;
    }
    
    void reset()
    {
        for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
            gainStates[c] = 1.0;
            muteStates[c] = false;
            soloStates[c] = false;
            channelLevel[c] = 0.0;
        }
    }
    
private:
    
    std::shared_ptr<audium::Playback> playback;
    
    juce::AudioBuffer<SampleType> audioBus;
    juce::AudioBuffer<SampleType> stereoBuffer;
        
    juce::dsp::Panner<SampleType> panners[MAX_AUDIO_CHANNELS];
    juce::dsp::Gain<SampleType> gains[MAX_AUDIO_CHANNELS];
    juce::dsp::Gain<SampleType> masterGain;
    
    std::atomic<float> channelLevel[MAX_AUDIO_CHANNELS];
    std::atomic<float> masterLevel[2];
    
    SampleType gainStates[MAX_AUDIO_CHANNELS];
    bool muteStates[MAX_AUDIO_CHANNELS];
    bool soloStates[MAX_AUDIO_CHANNELS];
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBusRenderer)

};

} // namespace audium
