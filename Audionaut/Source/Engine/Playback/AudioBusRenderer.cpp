//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioBusRenderer.h"
#include "Engine/AudiumEngine.h"

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
void AudioBusRenderer<SampleType>::prepareToPlay (int samplesPerBlockExpected, double sampleRate_)
{
    sampleRate = sampleRate_;
    recording->setSampleRate(sampleRate);
    audioBus.setSize(audioBus.getNumChannels(), samplesPerBlockExpected);
    
    stereoBuffer.setSize(2, samplesPerBlockExpected);

    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize    = samplesPerBlockExpected;
    spec.sampleRate          = sampleRate;
    
    for (auto i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
        
        spec.numChannels = 2;
        panners[i].prepare(spec);
        
        spec.numChannels = 1;
        gains[i].setRampDurationSeconds(0.01);
        gains[i].prepare(spec); 
    }
    
    spec.numChannels = 2;
    masterGain.setRampDurationSeconds(0.01);
    masterGain.prepare(spec);
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setChannelData(const int channelNumber, const AudioChannelData data)
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        audioChannelData[channelNumber] = data;
        setPan(channelNumber, data.pan);
        
        setGain(channelNumber, data.gain);
        setMute(channelNumber, data.mute);
        setSolo(channelNumber, data.solo);
    }
}

template <class SampleType>
const AudioChannelData AudioBusRenderer<SampleType>::getChannelData(const int channelNumber) const
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        return audioChannelData[channelNumber];
    }
    return AudioChannelData();
}

template <class SampleType>
void AudioBusRenderer<SampleType>::resetGains()
{
    // stupid code to avoid smoothing when start processing (start playing)
    for (auto i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
        auto rampDuration = gains[i].getRampDurationSeconds();
        gains[i].setRampDurationSeconds(0.0);
        gains[i].setGainLinear(gains[i].getGainLinear());
        gains[i].setRampDurationSeconds(rampDuration);
    }
    
    auto rampDuration = masterGain.getRampDurationSeconds();
    masterGain.setRampDurationSeconds(0.0);
    masterGain.setGainLinear(masterGain.getGainLinear());
    masterGain.setRampDurationSeconds(rampDuration);
}


template class AudioBusRenderer<float>;
template class AudioBusRenderer<double>;

} // namespace audium
