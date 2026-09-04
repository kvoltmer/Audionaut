//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "PlayListScheduler.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/AudioSources/ClipFadeSpec.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Core/DspClip.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeContainer.h"
#include "Engine/Core/LockFreeCommander.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Region/AudioRegionContainer.h"


using namespace::std::chrono;

using namespace::juce;

namespace audium {

void PlayListScheduler::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    externalSampleRate = sampleRate;
    bufferSize = samplesPerBlockExpected;
    voiceSourceContainer->prepareToPlay(samplesPerBlockExpected, sampleRate);
    playback->prepareToPlay(samplesPerBlockExpected, sampleRate);
    audioBusInterface->prepareToPlay(samplesPerBlockExpected, sampleRate);
    transportLoop->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

bool PlayListScheduler::scheduleClip(const audium::DspClip &dspClip,
                                     std::shared_ptr<VoiceSource> voiceSource,
                                     double transportPosition,
                                     int sampleOffset,
                                     int numSamples)
{
    auto context = audium::seconds;

    const auto spec = ClipFadeSpec::fromDspClip(dspClip, *tempoProvider);

    // the voice covers the audible span: the region window widened by the
    // fade extensions (head clamped at the file start)
    auto voiceAbsStart = dspClip.getAbsolutePosition(context) - dspClip.getHeadExtension(context);
    auto offset = voiceAbsStart - transportPosition;
    auto position = 0.0;
    auto startSample = 0;

    // one timeline second covers speedRatio seconds of source material
    const auto speedRatio = dspClip.getSpeedRatio();

    if (offset < 0.0) {
        // the clip already started: seek the speed-scaled distance into the file
        position = spec.voiceFileStart() - offset * speedRatio;

        // sample offset (loop)
        startSample = sampleOffset;
    }
    else {
        position = spec.voiceFileStart();
        startSample = static_cast<int>(std::round(offset * externalSampleRate));
        startSample += sampleOffset;
    }
    jassert(startSample <= numSamples);
    jassert(position >= 0.0);

    // source seconds left to play; the voice renders them in
    // duration / speedRatio timeline seconds
    auto duration = spec.voiceFileEnd() - position;
    jassert(duration >= 0.0);

    const auto secondsThisBuffer = static_cast<double>(numSamples) / externalSampleRate;
    jassert(context == audium::seconds);
    if (duration / speedRatio < secondsThisBuffer) {
        DBG("PlayListScheduler: duration samples " << int(duration / speedRatio * externalSampleRate) << " too small");
        return false;
    }

    voiceSource->setSpeedRatio(speedRatio);
    voiceSource->schedulePosition(position, startSample);
    voiceSource->scheduleDuration(duration / speedRatio, externalSampleRate);

    voiceSource->configureClipFades(spec, position, true);

    voiceSource->setGain(dspClip.dspClipData.clipGain);
    voiceSource->resetClipGain();

    return true;
}

void PlayListScheduler::process(double transportPositionClocks,
                                int numSamples,
                                const TransportLoop::LoopResult loopResult)
{
    // convert to seconds
    auto transportPosition = tempoProvider->clocksToSeconds(transportPositionClocks);
    
    if (loopResult.context == audium::clocks) {
        transportPosition = tempoProvider->clocksToSeconds(loopResult.positionResult);
    }
    else if (loopResult.context == audium::seconds) {
        transportPosition = loopResult.positionResult;
    }
    
    const auto secondsThisBuffer = static_cast<double>(numSamples) / externalSampleRate;
    auto transportRange = juce::Range<double> (transportPosition, transportPosition + secondsThisBuffer);
    
    auto clipsChanged = audioClipContainer->pull();
    auto dspClips = audioClipContainer->getConsumerObjects();
    
    for (auto clipData : dspClips) {
        
        const audium::DspClip dspClip(getTempoProvider(), clipData);
        
        if (dspClip.getRegionData(audium::seconds).isEmpty())
            continue;
        
        const auto voiceSource = voiceSourceContainer->getVoiceSourceAtIndex(dspClip.dspClipData.voiceSourceIndex);
        if (voiceSource == nullptr)
            continue;
        
        // the audible range includes the fade extensions - a head extension
        // must start the voice early, a tail extension must not be stopped
        // at the region end by the else branch below
        if (dspClip.getAudibleRange(audium::seconds).intersects(transportRange)) {
            
            
            if (clipsChanged &&
                voiceSource->isPlaying()) {
                voiceSource->stop(true);
            }
            
            if (loopResult.loopEvent) {
                // re-schedule clip
                if (not scheduleClip(dspClip,
                                     voiceSource,
                                     transportPosition,
                                     loopResult.numSamplesUntilLoop,
                                     numSamples)) {
                    continue;
                }
                
                // clip might start at loop start
                if (!voiceSource->isPlaying()) {
                    voiceSource->start();
                    playback->startVoice(voiceSource);
                }
            }
            
            if (not voiceSource->isPlaying()) {
                
                if (not scheduleClip(dspClip,
                                     voiceSource,
                                     transportPosition,
                                     0,
                                     numSamples)) {
                    continue;;
                }
                
                // TODO: why? please investigate
                if (not loopResult.loopEvent) {
                    voiceSource->setGain(dspClip.dspClipData.clipGain);
                }
                
                voiceSource->start();
                playback->startVoice(voiceSource);
            }
        }
        else {
            playback->stopVoice(voiceSource, false);
        }
    }
}

double PlayListScheduler::getTotalLength(audium::TimeContextType context, bool addOverhead) const
{
    auto totalLength = totalLengthClocks.load();
    
    if (addOverhead) {
        // in case transport is ahead
        totalLength = std::max(totalLength, getAbsolutePosition(audium::clocks));
        
        // add overhead to fit entire arrangement arrangement
        auto minimumLength = tempoProvider->secondsToClocks(60.0);
        totalLength = std::max(minimumLength, totalLength * 1.25);
    }
    
    if (context == audium::seconds)
        totalLength = tempoProvider->clocksToSeconds(totalLength);
    
    return totalLength;
}

void PlayListScheduler::startPlaying()
{
    if (linkEngine != nullptr) {
        commitPlayListData();
        linkEngine->setStartPlayingTime(getTempoProvider()->clocksToBeats(data.startPositionClocks));
        linkEngine->startPlaying();
        transportLoop->reset();
        transportLoop->setAbsoluteStartPosition(data.startPositionClocks, audium::clocks);
        audioBusInterface->resetGains();
    }
}

void PlayListScheduler::stopPlaying()
{
    if (linkEngine != nullptr) {
        linkEngine->stopPlaying();
    }
    
    if (isRecordingArmed()) {
        stopRecording();
    }
    
    audioBusInterface->stopAllVoices();
}

bool PlayListScheduler::isPlaying() const
{
    if (linkEngine != nullptr) {
        return linkEngine->isPlaying();
    }
    jassertfalse;
    return false;
}

double PlayListScheduler::getAbsolutePosition(audium::TimeContextType context) const
{
    if (context == audium::clocks) {
        return data.transportPositionClocks;
    }
    else if (context == audium::seconds) {
        return getTempoProvider()->clocksToSeconds(data.transportPositionClocks);
    }
    
    jassertfalse;
    return 0.0;
}

void PlayListScheduler::setAbsoluteStartPosition(double newPosition, audium::TimeContextType context)
{
    if (newPosition < 0.0)
        newPosition = 0.0;
    
    auto positionClocks = 0.0;
    if (context == audium::clocks) {
        positionClocks = newPosition;
    }
    else if (context == audium::seconds) {
        positionClocks = getTempoProvider()->secondsToClocks(newPosition);
    }
    
    data.startPositionClocks = positionClocks;
    
    if (!isPlaying()) {
        data.transportPositionClocks = positionClocks;
        
        transportLoop->setAbsoluteStartPosition(newPosition, context);
    }
}

double PlayListScheduler::getAbsoluteStartPosition(audium::TimeContextType context) const
{
    if (context == audium::clocks)
        return data.startPositionClocks;
    
    if (context == audium::seconds)
        return getTempoProvider()->clocksToSeconds(data.startPositionClocks);
    
    return 0.0;
}


int PlayListScheduler::getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioTrack> track)
{
    auto item = track->getPlayListContainer()->itemAtAbsolutePosition(getAbsolutePosition(audium::clocks), audium::clocks);
    if (item != nullptr) {
        return track->getPlayListContainer()->getPlayListItemIndex(item);
    }
    return -1;
}

void PlayListScheduler::setCurrentPositionAtPlayListItemIndex(std::shared_ptr<AudioTrack> track, int playListItemIndex)
{
    const auto item = track->getPlayListContainer()->getPlayListItem(playListItemIndex);
    if (item != nullptr) {
        data.startPositionClocks = item->getAbsolutePosition(audium::clocks);
        
        if (!isPlaying()) {
            data.transportPositionClocks = data.startPositionClocks;
        }
    }
}

double PlayListScheduler::getPlayListItemProgress(std::shared_ptr<AudioTrack> track, int playListItemIndex) const
{
    const auto item = track->getPlayListContainer()->getPlayListItem(playListItemIndex);
    if (item != nullptr &&
        not item->isRecording()) {
        auto context = audium::clocks;
        auto localPosition = item->absoluteToLocalPosition(getAbsolutePosition(context), context);
        auto progress = ((localPosition - item->getRegionData(context).getStart()) / item->getRegionData(context).getLength());
        // fade extensions place the transport outside the region window
        return juce::jlimit(0.0, 1.0, progress);
    }
    return 0.0;
}

void PlayListScheduler::bouncePlayListItem(juce::AudioFormatWriter* writer,
                                           std::shared_ptr<ExportAudioConfig> config,
                                           std::function<void ()> callback)
{
    jassert(config->playListItem != nullptr);
    jassert((int)config->sampleRate == (int)externalSampleRate);
    
    config->numChannels = config->playListItem->getPlayListContainer().getAudioTrack().getNumAudioTrackChannels();
    std::cout << "bounce channels: " << config->numChannels << std::endl;
    
    auto item = config->playListItem;

    // the exported span is the audible length: region window plus the fade
    // extensions; the portion of a head extension before the source file's
    // first sample is written as leading silence
    const auto spec = ClipFadeSpec::fromPlayListItem(*item);

    // audibleLength/preFileSilence are source seconds; the render happens at
    // the clip's speed
    auto totalSamples          = static_cast<int64>(spec.audibleLength() / spec.speedRatio * externalSampleRate);
    auto leadingSilenceSamples = static_cast<int64>(spec.preFileSilence() / spec.speedRatio * externalSampleRate);
    auto voiceSamples          = totalSamples - leadingSilenceSamples;
    auto iterations            = voiceSamples / config->blockSize;
    auto remainder             = voiceSamples - (iterations * config->blockSize);
    jassert(remainder < config->blockSize);

    AudioBuffer<float> buffer(config->numChannels, config->blockSize);
    AudioSourceChannelInfo info (&buffer, 0, config->blockSize);

    AudioBuffer<float> audioBusBuffer(config->numChannels, config->blockSize);
    juce::AudioSourceChannelInfo audioBusInfo (&audioBusBuffer, info.startSample, info.numSamples);

    for (auto source : config->playListItem->getVoiceSources()) {
        source->prepareToPlay(config->blockSize, config->sampleRate);
        source->setSpeedRatio(spec.speedRatio);
        source->schedulePosition(spec.voiceFileStart(), 0);
        source->scheduleDuration((spec.voiceFileEnd() - spec.voiceFileStart()) / spec.speedRatio, config->sampleRate);
        source->configureDynamics(config->playListItem);
        // snap the gain smoother - otherwise the export starts with a
        // 10 ms gain swell (the live scheduler does the same)
        source->resetClipGain();
        source->applyChannelMapping(false);
        source->start();
    }

    int64 samplesWritten = 0;

    // leading silence for the pre-file part of a head extension
    buffer.clear();
    auto silenceRemaining = leadingSilenceSamples;
    while (silenceRemaining > 0 && !config->userCanceled) {
        auto numSilence = static_cast<int>(juce::jmin(silenceRemaining, static_cast<int64>(config->blockSize)));
        writer->writeFromAudioSampleBuffer(buffer, 0, numSilence);
        samplesWritten += numSilence;
        silenceRemaining -= numSilence;
    }

    for (auto i = 0; i < iterations; ++i) {
        info.clearActiveBufferRegion();
        audioBusInfo.clearActiveBufferRegion();
        for (auto source : config->playListItem->getVoiceSources()) {
            source->getNextAudioBlock(audioBusInfo);
            for (auto c = 0; c < info.buffer->getNumChannels(); c++) {
                info.buffer->addFrom(c, info.startSample, audioBusBuffer.getReadPointer(c), info.numSamples);
            }
        }
    
        writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
        
        samplesWritten += info.numSamples;
        
        config->progress = i / (double)iterations;
        
        if (callback)
            callback();
        
        if (config->userCanceled)
            break;
    }
    
    if (!config->userCanceled) {
        
        // process remaining samples
        if (remainder > 0) {
            buffer.clear();
            info.numSamples = static_cast<int>(remainder);
            info.clearActiveBufferRegion();
            
            audioBusInfo.numSamples = static_cast<int>(remainder);
            audioBusInfo.clearActiveBufferRegion();
            for (auto source : config->playListItem->getVoiceSources()) {
                source->getNextAudioBlock(audioBusInfo);
                for (auto c = 0; c < info.buffer->getNumChannels(); c++) {
                    info.buffer->addFrom(c, info.startSample, audioBusBuffer.getReadPointer(c), info.numSamples);
                }
            }
            
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            samplesWritten += info.numSamples;
        }
        
        jassert(samplesWritten == totalSamples);
    }
    
}

void PlayListScheduler::bounceProject(juce::AudioFormatWriter* writer,
                                     std::shared_ptr<ExportAudioConfig> config,
                                     std::function<void ()> callback)
{
    jassert(config->playListItem == nullptr);
    
    // remember last position
    auto lastPosition = getAbsolutePosition(audium::seconds);
    setAbsoluteStartPosition(config->positionSeconds, audium::seconds);
    
    startPlaying();
    
    jassert((int)config->sampleRate == (int)externalSampleRate);
    auto length         = getTotalLength(audium::seconds) - config->positionSeconds;
    if (config->lengthSeconds > 0.0)
        length = config->lengthSeconds;
    auto totalSamples   = static_cast<int64>(length * externalSampleRate);
    auto iterations     = static_cast<int64>(totalSamples) / config->blockSize;
    auto remainder      = totalSamples - (iterations * config->blockSize);
    jassert(remainder < config->blockSize);
    
    auto positionBeats = TempoProvider::clocksToBeats(getAbsolutePosition(audium::clocks));
    
    AudioBuffer<float> outBuffer(config->numChannels, config->blockSize);
    AudioBuffer<float> inBuffer(config->numChannels, config->blockSize);
    AudioSourceChannelInfo info (&outBuffer, 0, config->blockSize);
    
    juce::dsp::AudioBlock<float> outBlock (outBuffer);
    juce::dsp::AudioBlock<float> inBlock (inBuffer);
    
    int64 samplesWritten = 0;
    for (auto i = 0; i < iterations; ++i) {
        const auto clocksThisBuffer = getTempoProvider()->secondsToClocks(static_cast<double>(config->blockSize) / externalSampleRate);
        const auto beatsThisBuffer = TempoProvider::clocksToBeats(clocksThisBuffer);
        
        juce::dsp::ProcessContextNonReplacing<float> context (inBlock, outBlock);
        inBlock.clear();
        outBlock.clear();
        process(context, true, positionBeats, config->blockSize);
        positionBeats += beatsThisBuffer;
        
        writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
        
        samplesWritten += info.numSamples;
        
        config->progress = i / (double)iterations;
        
        if (callback)
            callback();
        
        if (config->userCanceled)
            break;
    }
    
    if (!config->userCanceled) {
        
        // process remaining samples
        if (remainder > 0) {
            outBlock.clear();
            info.numSamples = static_cast<int>(remainder);
            juce::dsp::ProcessContextReplacing<float> context (outBlock);
            
            process(context, true, positionBeats, info.numSamples);
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            samplesWritten += info.numSamples;
        }
        
        jassert(samplesWritten == totalSamples);
    }
    
    setAbsoluteStartPosition(lastPosition, audium::seconds);
    stopPlaying();
}

std::vector<std::shared_ptr<PlayListItem>> PlayListScheduler::getPlayListItems(bool excludeSelectedItems) const
{
    std::vector<std::shared_ptr<PlayListItem>> result;
    for (auto track : audioTrackContainer->getAudioTracks()) {
        
        for (const auto &item : track->getPlayListContainer()->getPlayListItems()) {
            if (excludeSelectedItems && item->isSelected())
                continue;
            
            result.push_back(item);
        }
    }
    return result;
}

void PlayListScheduler::commitPlayListData()
{
    audioClipContainer->clear();
    
    for (auto track : audioTrackContainer->getAudioTracks()) {
        auto clips = track->getDspClipVector();
        for (auto clip : clips) {
            audioClipContainer->push_back(clip);
        }
    }
    
    // commit data
    audioClipContainer->commit();
    
    // update channel mapping
    voiceSourceContainer->applyChannelMapping();
    
    // calc total length
    double totalLength = 0.0;
    for (auto track : audioTrackContainer->getAudioTracks()) {
        totalLength = juce::jmax(totalLength, track->getTotalLength(audium::clocks));
    }
    totalLengthClocks = totalLength;
    //std::cout << "PlayListScheduler::commitPlayListData -> totalLengthClocks: " << totalLengthClocks << std::endl;
}

bool PlayListScheduler::anyTrackRecordEnabled() const
{
    for (auto track : getAudioTrackContainer()->getAudioTracks())
        if (track->isRecordEnabled())
            return true;
        
    return false;
}

void PlayListScheduler::startRecording(const int channelNumber)
{
    // only start recording one of the tracks is record-enabled
    if (anyTrackRecordEnabled()) {
        
        if (not isPlaying()) {
            // record from start position
            data.recordingStartPositionClocks = data.startPositionClocks;
        }
        else {
            // record on the fly so use the current transport position
            data.recordingStartPositionClocks = data.transportPositionClocks;
        }
        
        NullCheckedInvocation::invoke (onRecordingStartedFunction);
        
        data.isRecording = true;
        audioBusInterface->record(true,
                                  channelNumber,
                                  data.recordingStartPositionClocks);
    }
}

void PlayListScheduler::stopRecording(const int channelNumber)
{
    if (channelNumber < 0) {
        data.isRecording = false;
        setRecordingArmed(false);
        audioBusInterface->record(false);
    }
    else {
        audioBusInterface->record(false, channelNumber);
        if (not audioBusInterface->anyChannelRecording()) {
            data.isRecording = false;
        }
    }
    
    // tigger async message to load recorded audio files and create transport sources
    audioTrackContainer->sendActionMessage(audium::recordingFinishedAction);

}

double PlayListScheduler::getRecordingLength(audium::TimeContextType context) const
{
    auto length = data.transportPositionClocks - data.recordingStartPositionClocks;
    
    if (context == audium::seconds)
        length = tempoProvider->clocksToSeconds(length);
    
    return length;
}

bool PlayListScheduler::isRecording() const noexcept
{
    return data.isRecording;
}

#if CATCH2_TESTS
void PlayListScheduler::recordFromAudioBuffer(const AudioBuffer<float> &inputBuffer,
                                              double recLengthSeconds)
{
    auto blockSize = 512;
    auto sampleRate = 44100.0;
    
    prepareToPlay(blockSize, sampleRate);
    setAbsoluteStartPosition(0.0, audium::seconds);
    
    // arm for recording
    setRecordingArmed(true);
    auto track = getAudioTrackContainer()->getAudioTrack(0);
    jassert(track);
    track->setRecordEnabled(0, true);
    
    // sync 
    audioBusInterface->invokeCommands();
    
    // go for it:
    startRecording();
    startPlaying();
    
    jassert((int)sampleRate == (int)externalSampleRate);
    auto totalSamples   = static_cast<int64>(recLengthSeconds * sampleRate);
    jassert(totalSamples <= inputBuffer.getNumSamples());
    auto iterations     = static_cast<int64>(totalSamples) / blockSize;
    auto remainder      = totalSamples - (iterations * blockSize);
    jassert(remainder < blockSize);
    
    auto positionBeats = TempoProvider::clocksToBeats(getAbsolutePosition(audium::clocks));
    
    AudioBuffer<float> inBuffer(1, blockSize);
    AudioBuffer<float> outBuffer(1, blockSize);
    juce::dsp::AudioBlock<float> inBlock (inBuffer);
    juce::dsp::AudioBlock<float> outBlock (outBuffer);
    
    
    int64 samplesProcessed = 0;
    for (auto i = 0; i < iterations; ++i) {
        const auto clocksThisBuffer = getTempoProvider()->secondsToClocks(static_cast<double>(blockSize) / externalSampleRate);
        const auto beatsThisBuffer = TempoProvider::clocksToBeats(clocksThisBuffer);
        
        
        jassert(samplesProcessed < inputBuffer.getNumSamples());
        inBuffer.copyFrom(0, 0, inputBuffer, 0, (int)samplesProcessed, blockSize);
        
    
        juce::dsp::ProcessContextNonReplacing<float> context (inBlock, outBlock);
        
        outBlock.clear();
        process(context, true, positionBeats, blockSize);
        
        positionBeats += beatsThisBuffer;
        samplesProcessed += blockSize;
        
        // the threaded writer needs to catch up
        juce::Thread::sleep (1);
    }
    
    // process remaining samples
    if (remainder > 0) {
        outBlock.clear();
        
        blockSize = (int)remainder;
        prepareToPlay(blockSize, sampleRate);
        
        inBuffer.setSize(1, blockSize);
        outBuffer.setSize(1, blockSize);
        // need to re-assign
        inBlock = juce::dsp::AudioBlock<float>(inBuffer);
        outBlock = juce::dsp::AudioBlock<float>(outBuffer);

        
        jassert(samplesProcessed + blockSize <= inputBuffer.getNumSamples());
        inBuffer.copyFrom(0, 0, inputBuffer, 0, (int)(samplesProcessed), blockSize);
        
        juce::dsp::ProcessContextNonReplacing<float> context (inBlock, outBlock);
        process(context, true, positionBeats, blockSize);
        samplesProcessed += blockSize;
        
    }
    
    juce::Thread::sleep (1);
    
    jassert(samplesProcessed == totalSamples);
    
    
    stopRecording();
    stopPlaying();
    
    audioBusInterface->invokeCommands();
}
#endif // CATCH2_TESTS

} // namespace audium
