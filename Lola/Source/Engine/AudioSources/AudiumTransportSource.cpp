/*
  ==============================================================================

    AudiumTransportSource.cpp
    Created: 3 Nov 2024 11:26:27am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumTransportSource.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrackContainer.h"

AudiumTransportSource::AudiumTransportSource(AudioResource& audioResource,
                      std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource) :
    audioResource(audioResource),
    audioFormatReaderSource(audioFormatReaderSource)
{
    // the transport source
    audioTransportSource = std::make_shared<audium::AudioTransportSource>();

    // source
    auto readAheadBufferSize = 48000;
    auto readAheadThread = audioResource.getContainer().getReadAheadThread();
    auto memReader = dynamic_cast<MemoryMappedAudioFormatReader*>(audioFormatReaderSource->getAudioFormatReader());
    if (memReader)
    {
        readAheadBufferSize = 0;
        readAheadThread = nullptr;
    }

    audioTransportSource->setSource (audioFormatReaderSource.get(),
                                     readAheadBufferSize,
                                     readAheadThread,
                                     audioFormatReaderSource->getAudioFormatReader()->sampleRate,
                                     audioFormatReaderSource->getAudioFormatReader()->numChannels);
    
    // the channel remapping source
    channelRemapping = std::make_unique<juce::ChannelRemappingAudioSource>(audioTransportSource.get(), false);
    
    // main source is ChannelRemappingAudioSource
    mainSource = channelRemapping.get();
}

void AudiumTransportSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    if (mainSource != nullptr)
        mainSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudiumTransportSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (audioTransportSource->getBufferingSource() != nullptr &&
        audioTransportSource->getBufferingSource()->waitForNextAudioBlockReady(info, 2) == false)
    {
        std::cout << "waitForNextAudioBlockReady" << std::endl;
    }
    
    if (scheduledSample == 0)
    {
        mainSource->getNextAudioBlock(info);
        
        auto offset = 0;
        if (durationTimer.process(info.numSamples, offset))
        {
            stopIt();
        }
    }
    else
    {
        auto startSample = scheduledSample.load();
        scheduledSample.store(0);
        
        // process 1st part
        juce::AudioSourceChannelInfo infoPart1 (info.buffer, 0, startSample);
        mainSource->getNextAudioBlock(infoPart1);
        
        audioTransportSource->setPosition(scheduledPosition.load());
        //std::cout << "scheduledPosition " << scheduledPosition.load() << std::endl;
     
        // workaround. TODO: re-implement juce transportsource
        if (not audioTransportSource->isPlaying())
            audioTransportSource->start();
        
        // process 2nd part
        juce::AudioSourceChannelInfo infoPart2 (info.buffer, startSample, info.numSamples - startSample);
        mainSource->getNextAudioBlock(infoPart2);
    }
    
    // We need the number of channels of the actual file.
    auto numAudioFileChannels = (int)audioResource.getNumChannels();
    for (auto i = 0; i < std::min(info.buffer->getNumChannels(), numAudioFileChannels); i++)
    {
        outputLevel[i] = info.buffer->getMagnitude(i, info.startSample, info.numSamples);
    }
}

void AudiumTransportSource::applyChannelMapping()
{
    auto totalChannels = audioResource.getAudioTrack()->getAudioTrackContainer().getNumAudioTrackChannels();
    if (audioResource.getNumChannels() > totalChannels)
        totalChannels = audioResource.getNumChannels();
    
    channelRemapping->setNumberOfChannelsToProduce(totalChannels);
    
    channelRemapping->clearAllMappings();
    
    auto channelOffset = getAudioResource().getAudioTrack()->getChannelOffset();
    
    auto mapping = getAudioResource().getChannelMapping();
    
    auto numAudioFileChannels = audioResource.getNumChannels();
    
    for (auto source = 0; source < std::min((int)numAudioFileChannels, mapping.size()); source++) {
        channelRemapping->setOutputChannelMapping(source, mapping[source] + channelOffset);
    }
}
