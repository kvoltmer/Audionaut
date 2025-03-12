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
#include "Engine/Resource/ChannelMapping.h"

AudiumTransportSource::AudiumTransportSource(AudioResource& audioResource_,
                      std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource_) :
    audioResource(audioResource_),
    audioFormatReaderSource(audioFormatReaderSource_)
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
                                     static_cast<int>(audioFormatReaderSource->getAudioFormatReader()->numChannels));
    
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
        audioTransportSource->getBufferingSource()->waitForNextAudioBlockReady(info, 2) == false) {
        std::cout << "waitForNextAudioBlockReady" << std::endl;
    }
    
    if (scheduledStartSample.load() == 0) {
        auto offset = 0;
        if (durationTimer.process(info.numSamples, offset)) {
            // reached end of clip -> schedule stop
            audioTransportSource->stop();
            if (offset > 0) {
                AudioSourceChannelInfo infoStop (info);
                infoStop.numSamples = offset;
                mainSource->getNextAudioBlock(infoStop);
            }
            
        }
        else {
            mainSource->getNextAudioBlock(info);
        }
    }
    else
    {
        auto startSample = scheduledStartSample.load();
        scheduledStartSample.store(0);
        jassert(startSample < info.numSamples);
        
        audioTransportSource->setPosition(scheduledPosition.load());
        
        // process 2nd part
        AudioSourceChannelInfo infoPart2 (info.buffer, startSample, info.numSamples - startSample);
        mainSource->getNextAudioBlock(infoPart2);
        
        auto offset = 0;
        if (durationTimer.process(infoPart2.numSamples, offset))
        {
            jassertfalse; // hu?
            audioTransportSource->stop();
        }
        
    }
    
#if CATCH2_TESTS
    samplesProcessed += info.numSamples;
#endif
}

void AudiumTransportSource::applyChannelMapping()
{
    auto totalChannels = audioResource.getAudioTrack()->getAudioTrackContainer().getNumAudioTrackChannels();
    auto audioFileChannels = static_cast<int>(audioResource.getNumAudioFileChannels());
    if (audioFileChannels > totalChannels)
        totalChannels = audioFileChannels;
    
    channelRemapping->setNumberOfChannelsToProduce(totalChannels);
    
    channelRemapping->clearAllMappings();
    
    auto channelOffset = getAudioResource().getAudioTrack()->getChannelOffset();
    auto srcChannel = getAudioResource().getChannelMapping().getSourceChannel();
    auto dstChannel = getAudioResource().getChannelMapping().getDestinationChannel();
    jassert(srcChannel < audioFileChannels);
    jassert(dstChannel + channelOffset < totalChannels);
    channelRemapping->setOutputChannelMapping(srcChannel,
                                              dstChannel + channelOffset);
}
