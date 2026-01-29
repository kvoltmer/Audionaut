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

template <class SampleType>
void AudioBusRenderer<SampleType>::setChannelData(const int channelNumber, const AudioChannelData data)
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        audioChannelData[channelNumber] = data;
        setPan(channelNumber, data.pan);
        setMute(channelNumber, data.mute);
        setSolo(channelNumber, data.solo);
        setGain(channelNumber, data.gain);
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setRecordEnabled(const int channelNumber, bool bEnabled, std::shared_ptr<AudioRecorder> recorder)
{

    if (bEnabled) {
        if (recorders.find(channelNumber) == recorders.end()) {
            recorders.insert(std::make_pair(channelNumber, recorder));
        }
    }
    else {
        // erase recorder if exists at channel number
        if (recorders.find(channelNumber) != recorders.end()) {
            recorders.erase(channelNumber);
        }
    }
    
    audioChannelData[channelNumber].record = bEnabled;
    
//    std::cout << "All recorders:\n";
//    for (const auto& rec : recorders) {
//        std::cout << rec.first << " " <<  rec.second << std::endl;
//    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::record(bool start)
{
    auto take = AudiumEngine::recordingCounter;
    if (start)
        AudiumEngine::recordingCounter++;
    
    for (const auto& rec : recorders) {
        if (start)
            rec.second->startRecording(take, rec.first, sampleRate);
        else
            rec.second->stop();
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setRecordingThumbnail(AudioThumbnail *audioThumbnail,
                                                         int channelNumber)
{
    if (recorders.find(channelNumber) != recorders.end()) {
        recorders[channelNumber]->setAudioThumbnail(audioThumbnail);
    }
}

template <class SampleType>
std::shared_ptr<AudioRecorder> AudioBusRenderer<SampleType>::getAudioRecorder(int channelNumber)
{
    if (recorders.find(channelNumber) != recorders.end()) {
        return recorders[channelNumber];
    }
    return nullptr;
}

template <class SampleType>
void AudioBusRenderer<SampleType>::clearRecorders()
{
    recorders.clear();
}

template class AudioBusRenderer<float>;
template class AudioBusRenderer<double>;

} // namespace audium
