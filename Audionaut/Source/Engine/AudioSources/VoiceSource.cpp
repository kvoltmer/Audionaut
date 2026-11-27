//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "VoiceSource.h"
#include "Engine/AudioSources/ClipFadeSpec.h"
#include "Engine/AudioSources/RenderTiming.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/ChannelMapping.h"

namespace audium {

namespace {

/**
 * The standby cursor of a file that is not memory-mapped: a decoder of its
 * own, opened by the first read that asks for audio. That read comes from the
 * read-ahead thread once the lane is primed for a jump, so a clip costs one
 * open on the message thread instead of two, and a clip that never plays
 * never opens the second. Until then the transport keeps the lane parked at
 * the end of the file, where a read needs no decoder.
 *
 * Read by one thread at a time: the read-ahead thread once the lane's buffer
 * is running, whoever sets the chain up before that.
 */
class LazyReaderSource final : public juce::PositionableAudioSource
{
public:
    LazyReaderSource (std::shared_ptr<juce::AudioFormatManager> formatManager_, juce::URL url_,
                      std::shared_ptr<juce::AudioFormatReader> fallbackReader_) :
        formatManager (std::move (formatManager_)),
        url (std::move (url_)),
        fallbackReader (std::move (fallbackReader_)),
        totalLength (fallbackReader->lengthInSamples)
    {
    }

    void prepareToPlay (int, double) override {}
    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        const auto start = nextReadPosition.load();
        nextReadPosition.store (start + info.numSamples);

        if (start >= totalLength)
        {
            info.clearActiveBufferRegion();
            return;
        }

        if (readerSource == nullptr)
            open();

        if (readerSource->getNextReadPosition() != start)
            readerSource->setNextReadPosition (start);

        readerSource->getNextAudioBlock (info);
    }

    void setNextReadPosition (juce::int64 newPosition) override   { nextReadPosition.store (newPosition); }
    juce::int64 getNextReadPosition() const override              { return nextReadPosition.load(); }
    juce::int64 getTotalLength() const override                   { return totalLength; }
    bool isLooping() const override                               { return false; }

private:
    void open()
    {
        reader = AudioResourceContainer::openStreamingReader (*formatManager, url);

        if (reader == nullptr)
        {
            // audible only as what the own decoder is there to prevent: two
            // cursors seeking on one decoder around this voice's jumps
            juce::Logger::writeToLog ("VoiceSource: no standby decoder for " + url.toString (false)
                                      + ", the standby cursor shares the voice's");
            reader = fallbackReader;
        }

        readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader.get(), false);
    }

    const std::shared_ptr<juce::AudioFormatManager> formatManager;
    const juce::URL url;
    const std::shared_ptr<juce::AudioFormatReader> fallbackReader;
    const juce::int64 totalLength;
    std::atomic<juce::int64> nextReadPosition { 0 };
    std::shared_ptr<juce::AudioFormatReader> reader;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
};

} // namespace

VoiceSource::VoiceSource(AudioResource& audioResource_) :
audioResource(audioResource_),
audioFormatReader(audioResource_.audioFormatReader),
clipTransportSource(std::make_shared<ClipTransportSource>())
{
    jassert(audioFormatReader != nullptr);

    // memory-mapped readers need neither buffering nor a read-ahead thread
    auto readAheadBufferSize = 48000;
    const auto memoryMapped = dynamic_cast<const MemoryMappedAudioFormatReader*>(audioFormatReader.get()) != nullptr;

    if (memoryMapped) {
        readAheadBufferSize = 0;
    }
    else {
        auto& container = audioResource.getContainer();
        readAheadThread = container.getReadAheadThread();

        // A decoder keeps a position, and the voices of a file would take
        // turns on the resource's, each read seeking away from the last. Some
        // decoders do not land exactly after a seek (CoreAudio on a VBR MP3
        // without a VBR header), which made the render depend on how the
        // read-aheads happened to interleave. With a decoder per cursor each
        // one reads straight through
        if (auto ownReader = AudioResourceContainer::openStreamingReader(*container.getAudioFormatManager(), audioResource.getUrl()))
            audioFormatReader = std::move(ownReader);
        else
            juce::Logger::writeToLog("VoiceSource: no decoder of its own for " + audioResource.getUrl().toString(false)
                                     + ", the voice shares the file's");
    }

    audioFormatReaderSource = std::make_shared<juce::AudioFormatReaderSource>(audioFormatReader.get(), false);

    clipTransportSource->setSource (audioFormatReaderSource.get(),
                                     readAheadBufferSize,
                                     readAheadThread.get(),
                                     audioFormatReader->sampleRate,
                                     static_cast<int>(audioFormatReader->numChannels));

    // a second cursor for the transport's standby lane, which takes a known
    // jump - the loop wrap, a clip start - ahead of time: on a memory-mapped
    // reader (stateless per read, so the cursor costs nothing) it primes the
    // stretcher; on a buffered reader it fills a second read-ahead, from a
    // decoder of its own that the first prime opens. Both cursors read on
    // the read-ahead thread only
    if (memoryMapped)
        standbyReaderSource = std::make_shared<juce::AudioFormatReaderSource>(audioFormatReader.get(), false);
    else
        standbyReaderSource = std::make_shared<LazyReaderSource>(audioResource.getContainer().getAudioFormatManager(),
                                                                 audioResource.getUrl(), audioFormatReader);

    clipTransportSource->setStandbySource(standbyReaderSource.get());
}

void VoiceSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    clipTransportSource->prepareToPlay(samplesPerBlockExpected, sampleRate);

    // sized here, off the audio thread; renderChannel only grows it for a
    // block larger than announced
    renderBuffer.setSize(static_cast<int>(audioResource.getNumAudioFileChannels()),
                         samplesPerBlockExpected);
}

void VoiceSource::renderChannel (const juce::AudioSourceChannelInfo& info)
{
    // the transport renders at offset 0 of the voice's buffer, whatever
    // the caller's startSample: callers split a block at a loop wrap or
    // clip end
    if (info.numSamples > renderBuffer.getNumSamples())
        renderBuffer.setSize(renderBuffer.getNumChannels(), info.numSamples, false, false, true);

    juce::AudioSourceChannelInfo renderInfo (&renderBuffer, 0, info.numSamples);
    renderInfo.clearActiveBufferRegion();
    clipTransportSource->getNextAudioBlock(renderInfo);

    const auto src = sourceChannel.load();
    jassert(src < renderBuffer.getNumChannels());
    if (src < renderBuffer.getNumChannels() && info.buffer->getNumChannels() > 0)
        info.buffer->copyFrom(0, info.startSample, renderBuffer, src, 0, info.numSamples);
}

void VoiceSource::waitForReadAhead (const juce::AudioSourceChannelInfo& info)
{
    // A buffered chain plays silence for whatever its read-ahead does not
    // hold yet. Known jumps arrive through a lane filled ahead of time and
    // pass here at once; an unplanned one (a user seek, an edit restart)
    // gets the reader a moment - live the same bounded moment for every
    // buffered voice, offline as long as it takes, see RenderTiming.
    if (auto* buffering = clipTransportSource->getBufferingSource())
        if (! buffering->waitForNextAudioBlockReady(info, RenderTiming::inputReadinessTimeoutMs()))
            DBG("VoiceSource: buffering source not ready");
}

void VoiceSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    waitForReadAhead(info);

    const auto startSample = scheduledStartSample.load();

    if (startSample == 0) {
        auto samplesUntilStop = 0;
        if (durationTimer.process(info.numSamples, samplesUntilStop)) {

            AudioSourceChannelInfo infoStop1 (info);
            infoStop1.numSamples = samplesUntilStop;
            if (infoStop1.numSamples > 0)
                renderChannel(infoStop1);

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
            renderChannel(info);
        }
    }
    else if (startSample >= info.numSamples) {
        scheduledStartSample.store(startSample - info.numSamples);
        renderChannel(info);
    }
    else
    {
        scheduledStartSample.store(0);

        if (reScheduled) {
            // process 1st part (until start sample, in case we are already playing -> loop)
            AudioSourceChannelInfo infoPart1 (info.buffer, 0, startSample);
            renderChannel(infoPart1);
            reScheduled = false;
        }

        clipTransportSource->setPosition(scheduledPosition.load());

        // process 2nd part - from the read-ahead the jump landed in
        AudioSourceChannelInfo infoPart2 (info.buffer, startSample, info.numSamples - startSample);
        waitForReadAhead(infoPart2);
        renderChannel(infoPart2);

        auto offset = 0;
        if (durationTimer.process(infoPart2.numSamples, offset))
        {
            DBG("VoiceSource: stop right after scheduled start");
            clipTransportSource->stop(false);
        }
    }
}

void VoiceSource::applyChannelMapping(bool withChannelOffset)
{
    const auto audioFileChannels = static_cast<int>(audioResource.getNumAudioFileChannels());
    const auto totalChannels = std::max(audioResource.getAudioTrack()->getAudioTrackContainer().getNumAudioTrackChannels(),
                                        audioFileChannels);

    const auto& channelMapping = audioResource.getChannelMapping();
    const auto channelOffset = withChannelOffset ? audioResource.getAudioTrack()->getChannelOffset() : 0;
    const auto srcChannel = channelMapping.getSourceChannel();
    const auto dstChannel = channelMapping.getDestinationChannel();
    jassert(srcChannel < audioFileChannels);
    jassert(dstChannel + channelOffset < totalChannels);
    juce::ignoreUnused(audioFileChannels, totalChannels);

    // the voice renders file channel src into channel 0 of a one-channel
    // buffer, the caller adds that to the destination channel. Rendering
    // the whole bus width per voice made the block cost voices x channels,
    // quadratic in the track count
    sourceChannel.store(srcChannel);
    destinationChannel.store(dstChannel + channelOffset);
}


void VoiceSource::configureClipFades(const ClipFadeSpec& spec, double filePositionSeconds, bool reset)
{
    audium::configureClipFades(*clipTransportSource, spec, filePositionSeconds, reset);
}

void VoiceSource::configureDynamics(std::shared_ptr<PlayListItem> item)
{
    // Gain:
    auto channel    = audioResource.getChannelMapping().getDestinationChannel();
    auto gain       = item->getDynamics().getGain(channel);
    clipTransportSource->setGain(static_cast<float>(gain));

    // Fades: same shared configuration the scheduler uses, for a voice
    // scheduled at the audible start (see bouncePlayListItem)
    const auto spec = ClipFadeSpec::fromPlayListItem(*item);
    configureClipFades(spec, spec.voiceFileStart(), true);
}

} // namespace audium
