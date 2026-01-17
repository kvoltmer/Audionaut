//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioBusRenderer.h"


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

// TODO: only float supported at this time
template class AudioBusRenderer<float>;

// TODO: juce::AudioSourceChannelInfo must support double
//template class AudioBusRenderer<double>;

} // namespace audium
