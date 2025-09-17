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
#include "Engine/Group/AudioClip.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Core/DspClip.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Core/LockFreeContainer.h"
#include "Engine/Core/LockFreeCommander.h"
#include "Engine/PlayList/TransportLoop.h"

using namespace::std::chrono;

using namespace::juce;

namespace audium {

void PlayListScheduler::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    externalSampleRate = sampleRate;
    bufferSize = samplesPerBlockExpected;
    tempoProvider->prepareToPlay(samplesPerBlockExpected, sampleRate);
    transportSourceContainer->prepareToPlay(samplesPerBlockExpected, sampleRate);
    playback->prepareToPlay(samplesPerBlockExpected, sampleRate);
    audioBusInterface->prepareToPlay(samplesPerBlockExpected, sampleRate);
    transportLoop->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void PlayListScheduler::tick(bool isPlaying,
                             double beats,
                             int numSamples)
{
    if (isPlaying &&
        beats >= 0.0) {
        auto pos = TempoProvider::beatsToClocks(beats);
        auto onLoop = transportLoop->processLoop(pos, numSamples);
        data.transportPositionClocks = pos;
        process(data.transportPositionClocks, numSamples, onLoop);
    }
}

void PlayListScheduler::process(double transportPositionClocks, int numSamples, bool onLoop)
{
    // convert to seconds
    auto transportPosition = tempoProvider->clocksToSeconds(transportPositionClocks);
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
            
            if (clipsChanged || onLoop)
                transportSource->getAudioTransportSource()->stop();
            
            if (!transportSource->getAudioTransportSource()->isPlaying()) {
                
                auto absolute = dspClip.getAbsolutePosition(audium::seconds);
                auto local = dspClip.getRegionData(audium::seconds).getStart();
                auto offset = absolute - transportPosition;
                auto position = 0.0;
                auto startSamples = 0;
                
                if (offset < 0.0) {
                    position = local - offset;
                    
                    startSamples = 0; // startSamples is 0
                }
                else {
                    position = local;
                    
                    startSamples = static_cast<int>(offset * externalSampleRate);
                    jassert(startSamples < numSamples);
                }
                
                auto duration = dspClip.getRegionData(audium::seconds).getEnd() - position;
                
                jassert(position >= 0.0 && duration >= 0.0);
                
                transportSource->schedulePosition(position, startSamples);
                transportSource->scheduleDuration(duration, externalSampleRate);
                
                if (!onLoop) {
                    transportSource->getAudioTransportSource()->setGain(dspClip.dspClipData.clipGain);
                }
                auto fadeIn = tempoProvider->clocksToSeconds(dspClip.dspClipData.clipFadeInClocks);
                transportSource->getAudioTransportSource()->setFadeInSeconds(fadeIn, offset, true);
                auto fadeOut = tempoProvider->clocksToSeconds(dspClip.dspClipData.clipFadeOutClocks);
                transportSource->getAudioTransportSource()->setFadeOutSeconds(fadeOut, duration, true);
                
                transportSource->getAudioTransportSource()->start();
                playback->startVoice(transportSource);
                
                
                //                std::cout << "transport-pos: " << transportPosition << " ";
                //                std::cout << "clip-pos: " <<  absolute << " ";
                //                std::cout << "offset: " << offset << " ";
                //                std::cout << "file-pos: " << position << " ";
                //                std::cout << "duration: " << duration << " ";
                //                std::cout << "gain: " << dspClip.dspClipData.clipGain << " ";
                //                std::cout << std::endl;
                
                
            }
        }
        else if (onLoop) {
            // stop at loop end
            transportSource->getAudioTransportSource()->stop();
            
        }
    }
}

void PlayListScheduler::processAudio(const juce::AudioSourceChannelInfo& outputInfo)
{
    // TODO: avoid allocations in the audio thread ;/
    audioBusInterface->setNumAudioBusChannels(audioTrackContainer->getNumAudioTrackChannels());
    
    audioBusInterface->processAudio(outputInfo);
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
    }
}

void PlayListScheduler::stopPlaying()
{
    if (linkEngine != nullptr) {
        linkEngine->stopPlaying();
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
    if (item != nullptr) {
        auto context = audium::clocks;
        auto localPosition = item->absoluteToLocalPosition(getAbsolutePosition(context), context);
        auto progress = ((localPosition - item->getRegionData(context).getStart()) / item->getRegionData(context).getLength());
        return progress;
    }
    return 0.0;
}

void PlayListScheduler::bounceToFile(juce::AudioFormatWriter* writer,
                                     std::shared_ptr<ExportAudioConfig> config,
                                     std::function<void ()> callback)
{
    // remember last position
    auto lastPosition = getAbsolutePosition(audium::seconds);
    setAbsoluteStartPosition(config->positionSeconds, audium::seconds);
    
    startPlaying();
    
    jassert((int)config->sampleRate == (int)externalSampleRate);
    
    auto totalSamples   = static_cast<int64>((getTotalLength(audium::seconds) - config->positionSeconds) * externalSampleRate);
    auto iterations     = static_cast<int64>(totalSamples) / config->blockSize;
    auto remainder      = totalSamples - (iterations * config->blockSize);
    jassert(remainder < config->blockSize);
    
    auto positionBeats = TempoProvider::clocksToBeats(getAbsolutePosition(audium::clocks));
    
    AudioBuffer<float> buffer(config->numChannels, config->blockSize);
    AudioSourceChannelInfo info (&buffer, 0, config->blockSize);
    int64 samplesWritten = 0;
    for (auto i = 0; i < iterations; ++i) {
        const auto clocksThisBuffer = getTempoProvider()->secondsToClocks(static_cast<double>(config->blockSize) / externalSampleRate);
        const auto beatsThisBuffer = TempoProvider::clocksToBeats(clocksThisBuffer);
        
        tick(true, positionBeats, config->blockSize);
        positionBeats += beatsThisBuffer;
        
        
        buffer.clear();
        processAudio(info);
        
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
            processAudio(info);
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            samplesWritten += info.numSamples;
        }
        
        jassert(samplesWritten == totalSamples);
    }
    
    
    setAbsoluteStartPosition(lastPosition, audium::seconds);
    stopPlaying();
    
}

void PlayListScheduler::commitPlayListData()
{
    audioClipContainer->clear();
    
    for (auto track : audioTrackContainer->getAudioTracks()) {
        auto clips = track->getDspClipVector(isArrangementMode());
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
        totalLength = juce::jmax(totalLength, track->getTotalLength(audium::clocks, isArrangementMode()));
    }
    totalLengthClocks = totalLength;
    std::cout << "PlayListScheduler::commitPlayListData -> totalLengthClocks: " << totalLengthClocks << std::endl;
}

} // namespace audium
