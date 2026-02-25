//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "PlayListScheduler.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Core/DspClip.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
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
    transportSourceContainer->prepareToPlay(samplesPerBlockExpected, sampleRate);
    playback->prepareToPlay(samplesPerBlockExpected, sampleRate);
    audioBusInterface->prepareToPlay(samplesPerBlockExpected, sampleRate);
    transportLoop->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void PlayListScheduler::scheduleClip(const audium::DspClip &dspClip,
                                     std::shared_ptr<AudiumTransportSource> transportSource,
                                     double transportPosition,
                                     int sampleOffset,
                                     int numSamples)
{
    auto absolute = dspClip.getAbsolutePosition(audium::seconds);
    auto local = dspClip.getRegionData(audium::seconds).getStart();
    auto offset = absolute - transportPosition;
    auto position = 0.0;
    auto startSamples = 0;
    
    if (offset < 0.0) {
        position = local - offset;
        
        // sample offset (loop)
        startSamples = sampleOffset;
    }
    else {
        position = local;
        startSamples = static_cast<int>(offset * externalSampleRate);
        
    }
    //jassert(startSamples < numSamples);
    
    auto duration = dspClip.getRegionData(audium::seconds).getEnd() - position;
    
    jassert(position >= 0.0 && duration >= 0.0);
    
    transportSource->schedulePosition(position, startSamples);
    transportSource->scheduleDuration(duration, externalSampleRate);
    
//    if (not loopResult.loopEvent) {
//        transportSource->getAudioTransportSource()->setGain(dspClip.dspClipData.clipGain);
//    }
    auto fadeIn = tempoProvider->clocksToSeconds(dspClip.dspClipData.clipFadeInClocks);
    transportSource->getAudioTransportSource()->setFadeInSeconds(fadeIn, offset, true);
    auto fadeOut = tempoProvider->clocksToSeconds(dspClip.dspClipData.clipFadeOutClocks);
    transportSource->getAudioTransportSource()->setFadeOutSeconds(fadeOut, duration, true);
    
    //                std::cout << "transport-pos: " << transportPosition << " ";
    //                std::cout << "clip-pos: " <<  absolute << " ";
    //                std::cout << "offset: " << offset << " ";
    //                std::cout << "file-pos: " << position << " ";
    //                std::cout << "duration: " << duration << " ";
    //                std::cout << "gain: " << dspClip.dspClipData.clipGain << " ";
    //                std::cout << std::endl;
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
        const auto transportSource = transportSourceContainer->getTransportSourceAtIndex(dspClip.dspClipData.transportSourceIndex);
        if (transportSource == nullptr)
            continue;
        
        if (dspClip.getAbsolutePositionRange(audium::seconds).intersects(transportRange)) {
            
            
            if (clipsChanged) {
                transportSource->getAudioTransportSource()->stop();
            }
            else if (loopResult.loopEvent) {
                // TODO: stop in
                //transportSource->getAudioTransportSource()->stop();
                
                // (re) schedule
                auto secondsUntilLoop = loopResult.timeUntilLoop;
                if (loopResult.context == clocks)
                    secondsUntilLoop = tempoProvider->clocksToSeconds(loopResult.timeUntilLoop);
                
                scheduleClip(dspClip,
                             transportSource,
                             transportPosition + secondsUntilLoop,
                             loopResult.numSamplesUntilLoop,
                             numSamples);
            }
            
            if (!transportSource->getAudioTransportSource()->isPlaying()) {
                
                scheduleClip(dspClip,
                             transportSource,
                             transportPosition,
                             0,
                             numSamples);
                
                // TODO: why? please investigate
                if (not loopResult.loopEvent) {
                    transportSource->getAudioTransportSource()->setGain(dspClip.dspClipData.clipGain);
                }
                
                transportSource->getAudioTransportSource()->start();
                playback->startVoice(transportSource);
            }
        }
        else {
            playback->stopVoice(transportSource);
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
    
    playback->stopAllVoices();
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
        return progress;
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
    
    auto totalSamples   = static_cast<int64>((config->playListItem->getRegionData(audium::seconds).getLength()) * externalSampleRate);
    auto iterations     = static_cast<int64>(totalSamples) / config->blockSize;
    auto remainder      = totalSamples - (iterations * config->blockSize);
    jassert(remainder < config->blockSize);
    
    AudioBuffer<float> buffer(config->numChannels, config->blockSize);
    AudioSourceChannelInfo info (&buffer, 0, config->blockSize);
    
    AudioBuffer<float> audioBusBuffer(config->numChannels, config->blockSize);
    juce::AudioSourceChannelInfo audioBusInfo (&audioBusBuffer, info.startSample, info.numSamples);
    
    auto item = config->playListItem;
    
    for (auto source : config->playListItem->getTransportSources()) {
        source->prepareToPlay(config->blockSize, config->sampleRate);
        source->schedulePosition(config->playListItem->getRegionData(seconds).getStart(), 0);
        source->scheduleDuration(config->playListItem->getRegionData(seconds).getLength(), config->sampleRate);
        source->configureDynamics(config->playListItem, tempoProvider);
        source->applyChannelMapping(false);
        source->getAudioTransportSource()->start();
    }
    
    int64 samplesWritten = 0;
    for (auto i = 0; i < iterations; ++i) {
        info.clearActiveBufferRegion();
        audioBusInfo.clearActiveBufferRegion();
        for (auto source : config->playListItem->getTransportSources()) {
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
            for (auto source : config->playListItem->getTransportSources()) {
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
    transportSourceContainer->applyChannelMapping();
    
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
        
        for (auto track : getAudioTrackContainer()->getAudioTracks()) {
            
            if (track->isRecordEnabled() &&
                not track->isRecording()) {
                
                auto resourceGroup = track->createNewResourceGroup();
                std::vector<std::shared_ptr<AudioResource>> resources;
                
                for (auto chan : track->audioChannelContainer->objects) {
                    if (chan->isRecordEnabled()) {
                        auto destChannel = chan->getChannelNumber();
                        auto url = URL (juce::File());
                        auto audioResource = track->getAudioResourceContainer().addAudioResource(url,
                                                                                                 nullptr,
                                                                                                 track,
                                                                                                 resourceGroup,
                                                                                                 destChannel,
                                                                                                 0);
                        resources.push_back(audioResource);
                    }
                }
                
                auto newItem = track->createDefaultPlayListItem(resources.front(),
                                                 resourceGroup,
                                                 data.recordingStartPositionClocks,
                                                 audium::clocks);
                newItem->needsLengthUpdate = true;
                
            }
        }
        
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

} // namespace audium
