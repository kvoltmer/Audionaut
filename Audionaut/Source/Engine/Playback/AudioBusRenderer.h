//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "PlaybackDefines.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Channel/AudioChannelData.h"

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
        jassert(inputChannels > 0);
        
        juce::dsp::AudioBlock<SampleType> audioBusBlock(audioBus);
        auto audioBusChannels = audioBus.getNumChannels();
                
        if (audioBusChannels > 0) {
            
            // render the entire bus (all channels)
            audioBus.clear();
            
            // process playback
            juce::dsp::ProcessContextReplacing<SampleType> audioBusContext(audioBusBlock);
            playback->process(audioBusContext);
            
            // add input to audio bus
            for (auto i = 0; i < inputChannels; i++) {
                if (audioChannelData[i].monitor) {
                    if (i < audioBusChannels) {
                        audioBusBlock.getSingleChannelBlock(i).add( inputBlock.getSingleChannelBlock(i) );
                    }
                }
            }
            
            // process gain for each audio bus channel
            for (auto i = 0; i < audioBusChannels; i++) {
                auto channelBlock = audioBusBlock.getSingleChannelBlock(i);
                juce::dsp::ProcessContextReplacing<SampleType> gainContext( channelBlock );
                gains[i].process(gainContext);
                channelLevel[i] = channelBlock.findMinAndMax().getEnd();
            }
            
            // mono output
            if (outputChannels == 1) {
                for (auto i = 0; i < audioBusChannels; i++) {
                    // add from audio bus to mono output
                    outputBlock.getSingleChannelBlock(0).add( audioBusBlock.getSingleChannelBlock(i) );
                }
                
            } // stereo
            else if (outputChannels == 2) {
                for (auto i = 0; i < audioBusChannels; i++) {

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
                jassert(outputChannels == audioBusChannels);
                for (auto c = 0; c < std::min((int)outputChannels, audioBusChannels); c++) {
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
            if (audioChannelData[channelNumber].solo)
                gains[channelNumber].setGainLinear(newGain);
            else if (!audioChannelData[channelNumber].mute)
                gains[channelNumber].setGainLinear(newGain);
            audioChannelData[channelNumber].gain = newGain;
        }
    }
    
    void setMute(const int channelNumber, const bool bMute)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            //std::cout << "setMute " << channelNumber << " " << bMute << std::endl;
            audioChannelData[channelNumber].mute = bMute;
            if (!audioChannelData[channelNumber].solo)
                gains[channelNumber].setGainLinear(bMute ? 0.f : audioChannelData[channelNumber].gain);
        }
    }

    void setSolo(const int channelNumber, const bool bSolo)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            //std::cout << "setSolo " << channelNumber << " " << bSolo << std::endl;
            audioChannelData[channelNumber].solo = bSolo;

            auto anySolo = false;
            for (auto c = 0; c < audioBus.getNumChannels(); ++c) {
                if (audioChannelData[c].solo) {
                    anySolo = true;
                    break;
                }
            }
            
            for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
                if (anySolo) {
                    if (audioChannelData[c].solo)
                        gains[c].setGainLinear(audioChannelData[c].gain);
                    else
                        gains[c].setGainLinear(0.f);
                }
                else {
                    // mute stats apply
                    gains[c].setGainLinear(audioChannelData[c].mute ? 0.f : audioChannelData[c].gain);
                }
            }
        }
    }
    
    void setChannelData(const int channelNumber, const AudioChannelData data)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            audioChannelData[channelNumber] = data;
            setPan(channelNumber, data.pan);
            setMute(channelNumber, data.mute);
            setSolo(channelNumber, data.solo);
            setGain(channelNumber, data.gain);
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
    
private:
    
    std::shared_ptr<audium::Playback> playback;
    
    juce::AudioBuffer<SampleType> audioBus;
    juce::AudioBuffer<SampleType> stereoBuffer;
        
    juce::dsp::Panner<SampleType> panners[MAX_AUDIO_CHANNELS];
    juce::dsp::Gain<SampleType> gains[MAX_AUDIO_CHANNELS];
    juce::dsp::Gain<SampleType> masterGain;
    
    std::atomic<float> masterLevel[2];
    std::atomic<float> channelLevel[MAX_AUDIO_CHANNELS];
    
    AudioChannelData audioChannelData[MAX_AUDIO_CHANNELS];
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBusRenderer)

};

} // namespace audium
