//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumTransportSource.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/ChannelMapping.h"

namespace audium {

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
        auto samplesUntilStop = 0;
        if (durationTimer.process(info.numSamples, samplesUntilStop)) {
            
            AudioSourceChannelInfo infoStop1 (info);
            infoStop1.numSamples = samplesUntilStop;
            if (infoStop1.numSamples > 0)
                mainSource->getNextAudioBlock(infoStop1);

            // reached end of clip -> apply stop
            audioTransportSource->stop(false);
            
            AudioSourceChannelInfo infoStop2 (info);
            infoStop2.startSample = samplesUntilStop;
            infoStop2.numSamples = info.numSamples - samplesUntilStop;
            if (infoStop2.numSamples > 0)
                mainSource->getNextAudioBlock(infoStop2);
            
        }
        else {
            mainSource->getNextAudioBlock(info);
        }
    }
    else if (scheduledStartSample.load() >= info.numSamples) {
        scheduledStartSample.store(scheduledStartSample.load() - info.numSamples);
        mainSource->getNextAudioBlock(info);
    }
    else
    {
        auto startSample = scheduledStartSample.load();
        scheduledStartSample.store(0);
        jassert(startSample < info.numSamples);
    
        if (reScheduled) {
            // process 1st part (until start sample, in case we are already playing -> loop)
            AudioSourceChannelInfo infoPart1 (info.buffer, 0, startSample);
            mainSource->getNextAudioBlock(infoPart1);
            reScheduled = false;
        }

        audioTransportSource->setPosition(scheduledPosition.load());
        
        // process 2nd part
        AudioSourceChannelInfo infoPart2 (info.buffer, startSample, info.numSamples - startSample);
        mainSource->getNextAudioBlock(infoPart2);
        
        auto offset = 0;
        if (durationTimer.process(infoPart2.numSamples, offset))
        {
            std::cout << "AudiumTransportSource::getNextAudioBlock -> stop right after start!" << std::endl;
            audioTransportSource->stop(false);
        }
        
    }
}

void AudiumTransportSource::applyChannelMapping(bool withChannelOffset)
{
    auto totalChannels = audioResource.getAudioTrack()->getAudioTrackContainer().getNumAudioTrackChannels();
    auto audioFileChannels = static_cast<int>(audioResource.getNumAudioFileChannels());
    if (audioFileChannels > totalChannels)
        totalChannels = audioFileChannels;
    
    channelRemapping->setNumberOfChannelsToProduce(totalChannels);
    
    channelRemapping->clearAllMappings();
    
    auto channelOffset = withChannelOffset ? getAudioResource().getAudioTrack()->getChannelOffset() : 0;
    auto srcChannel = getAudioResource().getChannelMapping().getSourceChannel();
    auto dstChannel = getAudioResource().getChannelMapping().getDestinationChannel();
    jassert(srcChannel < audioFileChannels);
    jassert(dstChannel + channelOffset < totalChannels);
    channelRemapping->setOutputChannelMapping(srcChannel,
                                              dstChannel + channelOffset);
}


void AudiumTransportSource::configureDynamics(std::shared_ptr<PlayListItem> item,
                                              std::shared_ptr<TempoProvider> tempoProvider)
{
    // Gain:
    auto channel    = getAudioResource().getChannelMapping().getDestinationChannel();
    auto gain       = item->getClipGain(channel);
    getAudioTransportSource()->setGain(static_cast<float>(gain));
    
    // Fade-in
    auto fadeIn = tempoProvider->clocksToSeconds(item->getFadeInClocks());
    auto offset = 0;
    getAudioTransportSource()->setFadeInSeconds(fadeIn, offset, true);
    
    // Fade-out
    auto fadeOut = tempoProvider->clocksToSeconds(item->getFadeOutClocks());
    getAudioTransportSource()->setFadeOutSeconds(fadeOut, item->getRegionData(audium::seconds).getLength(), true);
}

} // namespace audium
