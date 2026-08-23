//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumTransportSource.h"
#include "Engine/AudioSources/ClipFadeSpec.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/ChannelMapping.h"

namespace audium {

AudiumTransportSource::AudiumTransportSource(AudioResource& audioResource_,
                                             std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource_) :
audioResource(audioResource_),
audioFormatReaderSource(std::move(audioFormatReaderSource_)),
clipTransportSource(std::make_shared<ClipTransportSource>())
{
    const auto* reader = audioFormatReaderSource->getAudioFormatReader();

    // memory-mapped readers need neither buffering nor a read-ahead thread
    auto readAheadBufferSize = 48000;
    auto readAheadThread = audioResource.getContainer().getReadAheadThread();
    if (dynamic_cast<const MemoryMappedAudioFormatReader*>(reader) != nullptr)
    {
        readAheadBufferSize = 0;
        readAheadThread = nullptr;
    }

    clipTransportSource->setSource (audioFormatReaderSource.get(),
                                     readAheadBufferSize,
                                     readAheadThread,
                                     reader->sampleRate,
                                     static_cast<int>(reader->numChannels));

    channelRemapping = std::make_unique<juce::ChannelRemappingAudioSource>(clipTransportSource.get(), false);
}

void AudiumTransportSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    channelRemapping->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudiumTransportSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (clipTransportSource->getBufferingSource() != nullptr &&
        clipTransportSource->getBufferingSource()->waitForNextAudioBlockReady(info, 2) == false) {
        DBG("AudiumTransportSource: buffering source not ready");
    }

    const auto startSample = scheduledStartSample.load();

    if (startSample == 0) {
        auto samplesUntilStop = 0;
        if (durationTimer.process(info.numSamples, samplesUntilStop)) {

            AudioSourceChannelInfo infoStop1 (info);
            infoStop1.numSamples = samplesUntilStop;
            if (infoStop1.numSamples > 0)
                channelRemapping->getNextAudioBlock(infoStop1);

            // reached end of clip -> apply stop
            clipTransportSource->stop(false);

            // the clip has ended: the remainder of the buffer must stay
            // silent instead of bleeding audio from beyond the clip end
            AudioSourceChannelInfo infoStop2 (info);
            infoStop2.startSample = samplesUntilStop;
            infoStop2.numSamples = info.numSamples - samplesUntilStop;
            if (infoStop2.numSamples > 0)
                infoStop2.clearActiveBufferRegion();
        }
        else {
            channelRemapping->getNextAudioBlock(info);
        }
    }
    else if (startSample >= info.numSamples) {
        scheduledStartSample.store(startSample - info.numSamples);
        channelRemapping->getNextAudioBlock(info);
    }
    else
    {
        scheduledStartSample.store(0);

        if (reScheduled) {
            // process 1st part (until start sample, in case we are already playing -> loop)
            AudioSourceChannelInfo infoPart1 (info.buffer, 0, startSample);
            channelRemapping->getNextAudioBlock(infoPart1);
            reScheduled = false;
        }

        clipTransportSource->setPosition(scheduledPosition.load());

        // process 2nd part
        AudioSourceChannelInfo infoPart2 (info.buffer, startSample, info.numSamples - startSample);
        channelRemapping->getNextAudioBlock(infoPart2);

        auto offset = 0;
        if (durationTimer.process(infoPart2.numSamples, offset))
        {
            DBG("AudiumTransportSource: stop right after scheduled start");
            clipTransportSource->stop(false);
        }
    }
}

void AudiumTransportSource::applyChannelMapping(bool withChannelOffset)
{
    const auto audioFileChannels = static_cast<int>(audioResource.getNumAudioFileChannels());
    const auto totalChannels = std::max(audioResource.getAudioTrack()->getAudioTrackContainer().getNumAudioTrackChannels(),
                                        audioFileChannels);

    channelRemapping->setNumberOfChannelsToProduce(totalChannels);
    channelRemapping->clearAllMappings();

    const auto& channelMapping = audioResource.getChannelMapping();
    const auto channelOffset = withChannelOffset ? audioResource.getAudioTrack()->getChannelOffset() : 0;
    const auto srcChannel = channelMapping.getSourceChannel();
    const auto dstChannel = channelMapping.getDestinationChannel();
    jassert(srcChannel < audioFileChannels);
    jassert(dstChannel + channelOffset < totalChannels);
    channelRemapping->setOutputChannelMapping(srcChannel,
                                              dstChannel + channelOffset);
}


void AudiumTransportSource::configureDynamics(std::shared_ptr<PlayListItem> item)
{
    // Gain:
    auto channel    = audioResource.getChannelMapping().getDestinationChannel();
    auto gain       = item->getDynamics().getGain(channel);
    clipTransportSource->setGain(static_cast<float>(gain));

    // Fades: same shared configuration the scheduler uses, for a voice
    // scheduled at the audible start (see bouncePlayListItem)
    ClipFadeSpec spec;
    auto regionSeconds = item->getRegionData(audium::seconds);
    spec.regionStart = regionSeconds.getStart();
    spec.regionEnd   = regionSeconds.getEnd();
    spec.fadeIn      = item->getDynamics().getFadeIn(audium::seconds);
    spec.fadeOut     = item->getDynamics().getFadeOut(audium::seconds);
    spec.fadeInStart = item->getDynamics().getFadeInStart(audium::seconds);
    spec.fadeOutEnd  = item->getDynamics().getFadeOutEnd(audium::seconds);
    spec.fadeInCurve  = item->getDynamics().getFadeInCurve();
    spec.fadeOutCurve = item->getDynamics().getFadeOutCurve();

    configureClipFades(*clipTransportSource, spec, spec.voiceFileStart(), true);
}

} // namespace audium
