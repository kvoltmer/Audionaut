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
#include "Engine/Recording/Recording.h"
#include "Engine/Recording/AudioRecorder.h"

using namespace juce::dsp;

namespace audium
{
class AudioThumbnail;

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
    AudioBusRenderer(std::shared_ptr<audium::Playback> playback_,
                     std::shared_ptr<Recording> recording_) :
        playback(playback_),
        recording(recording_)
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
        
        AudioBlock<SampleType> audioBusBlock(audioBus);
        auto audioBusChannels = audioBus.getNumChannels();
                
        if (audioBusChannels > 0) {
            
            // render the entire bus (all channels)
            audioBusBlock.clear();
            

            
            // add input to audio bus, each bus channel fed by its effective hardware input
            for (auto k = 0; k < audioBusChannels; k++) {
                const auto effectiveInput = AudioChannelData::effectiveInputChannel(audioChannelData[k]);
                if (effectiveInput < 0 || effectiveInput >= (int) inputChannels)
                    continue;

                if (audioChannelData[k].monitor) {
                    audioBusBlock.getSingleChannelBlock(k).add( inputBlock.getSingleChannelBlock(effectiveInput) );
                }

                if (audioChannelData[k].record) {
                    recordingLevel[k] = std::abs(inputBlock.getSingleChannelBlock(effectiveInput).findMinAndMax().getEnd());

                    if (auto recorder = recording->getAudioRecorder(k)) {
                        auto input = inputBlock.getSingleChannelBlock(effectiveInput);
                        auto output = audioBusBlock.getSingleChannelBlock(k);
                        ProcessContextNonReplacing<SampleType> recContext(input, output);
                        recorder->process( recContext );
                    }
                }
            }
            
            // process playback, don't overwrite input -> ProcessContextNonReplacing
            ProcessContextNonReplacing<SampleType> audioBusContext(inputBlock, audioBusBlock);
            playback->process(audioBusContext);
            
            // process gain for each audio bus channel
            for (auto i = 0; i < audioBusChannels; i++) {
                auto channelBlock = audioBusBlock.getSingleChannelBlock(i);
                ProcessContextReplacing<SampleType> gainContext( channelBlock );
                gains[i].process(gainContext);
                channelLevel[i] = std::abs(channelBlock.findMinAndMax().getEnd());
            }
            
            if (stemExport.load()) {
                // offline bounce: identity copy, one output channel per bus channel
                for (auto c = 0; c < std::min((int)outputChannels, audioBusChannels); c++) {
                    outputBlock.getSingleChannelBlock(c).add( audioBusBlock.getSingleChannelBlock(c) );
                }
            }
            else {
                const bool stereoMain = outputChannels >= 2;
                AudioBlock<SampleType> mainMixBlock (mainMixBuffer);
                if (stereoMain)
                    mainMixBlock.clear();

                for (auto i = 0; i < audioBusChannels; i++) {

                    auto channelBlock = audioBusBlock.getSingleChannelBlock(i);
                    const auto destination = audioChannelData[i].outputChannel;

                    if (destination < 0) { // Main: pan into the stereo main mix
                        if (stereoMain) {
                            stereoBuffer.clear();
                            AudioBlock<SampleType> stereoBlock (stereoBuffer);

                            // process stereo pan
                            ProcessContextNonReplacing<SampleType> panContext(channelBlock,
                                                                              stereoBlock);
                            panners[i].process(panContext);

                            for (auto c = 0; c < 2; c++) {
                                mainMixBlock.getSingleChannelBlock(c).add( stereoBlock.getSingleChannelBlock(c) );
                            }
                        }
                        else if (outputChannels == 1) {
                            // mono device: sum the main mix into the single output
                            outputBlock.getSingleChannelBlock(0).add( channelBlock );
                        }
                    }
                    else if (destination < (int) outputChannels) {
                        // direct mono out: post channel-gain, no pan, no master gain
                        outputBlock.getSingleChannelBlock(destination).add( channelBlock );
                    }
                    // else: routed beyond the current device's outputs -> dropped
                }

                if (stereoMain) {
                    // master gain + level metering apply to the main mix only
                    ProcessContextReplacing<SampleType> gainContext(mainMixBlock);
                    masterGain.process(gainContext);

                    for (auto m = 0; m < 2; ++m) {
                        auto minmax = mainMixBlock.getSingleChannelBlock(m).findMinAndMax();
                        masterLevel[m].store( std::abs(minmax.getEnd()) );
                        outputBlock.getSingleChannelBlock(m).add( mainMixBlock.getSingleChannelBlock(m) );
                    }
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
    
    void setChannelData(const int channelNumber, const AudioChannelData data);
    
    const AudioChannelData getChannelData(const int channelNumber) const;
    
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

    const float getRecordingLevel(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            return recordingLevel[channelNumber].load();
        
        return 0.f;
    }
    
    const float getMasterLevel(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < 2)
            return masterLevel[channelNumber].load();
        
        return 0.f;
    }
    
    std::shared_ptr<Recording> getRecording() const { return recording; }
    
    std::shared_ptr<Playback> getPlayback() const { return playback; }
    
    void setRecordEnabled(int channelNumber, bool bEnabled) { audioChannelData[channelNumber].record = bEnabled; }

    /**
     * @brief Enables the offline bounce mapping: identity copy of bus channels to output channels,
     * bypassing output routing. Only set while the live device callback is bypassed.
     */
    void setStemExport(bool bStemExport) { stemExport.store(bStemExport); }

    void resetGains();
    
    const Gain<SampleType>& getGain(int channel) { return gains[channel]; }
    
private:
    
    std::shared_ptr<audium::Playback> playback;
    
    std::shared_ptr<Recording> recording;
    
    juce::AudioBuffer<SampleType> audioBus;
    juce::AudioBuffer<SampleType> stereoBuffer;
    juce::AudioBuffer<SampleType> mainMixBuffer;

    std::atomic<bool> stemExport { false };

    Panner<SampleType> panners[MAX_AUDIO_CHANNELS];
    Gain<SampleType> gains[MAX_AUDIO_CHANNELS];
    Gain<SampleType> masterGain;
    
    std::atomic<float> masterLevel[2];
    std::atomic<float> channelLevel[MAX_AUDIO_CHANNELS];
    std::atomic<float> recordingLevel[MAX_AUDIO_CHANNELS];
    
    AudioChannelData audioChannelData[MAX_AUDIO_CHANNELS];
    
    double sampleRate = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBusRenderer)

};

} // namespace audium
