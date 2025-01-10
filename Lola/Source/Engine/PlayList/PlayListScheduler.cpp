/*
  ==============================================================================

    PlayListScheduler.cpp
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
#include "Engine/Core/LockFreeContainer.h"


using namespace::std::chrono;

using namespace::juce;

void PlayListScheduler::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    externalSampleRate = sampleRate;
    bufferSize = samplesPerBlockExpected;
    tempoProvider->prepareToPlay(samplesPerBlockExpected, sampleRate);
    transportSourceContainer->prepareToPlay(samplesPerBlockExpected, sampleRate);
    playback->prepareToPlay(samplesPerBlockExpected, sampleRate);
    audioBusRenderer->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void PlayListScheduler::tick(bool isPlaying,
                             double beats,
                             int numSamples)
{
    if (isPlaying)
    {
        //auto beats = sessionState->beatAtTime(beginHostTime, quantum);
        if (beats >= 0.)
        {
            // assign absolute position
            if (data.loopPlayList)
            {
                data.transportPositionClocks = std::fmod(TempoProvider::beatsToClocks(beats), getTotalLength(audium::clocks));
            }
            else
            {
                data.transportPositionClocks = TempoProvider::beatsToClocks(beats);
            }
            
            process(data.transportPositionClocks, numSamples);
        }
    }
}

void PlayListScheduler::process(double transportPositionClocks, int numSamples)
{
    // convert to seconds
    auto transportPosition = tempoProvider->clocksToSeconds(transportPositionClocks);
    const auto secondsThisBuffer = static_cast<double>(numSamples) / externalSampleRate;
    auto transportRange = juce::Range<double> (transportPosition, transportPosition + secondsThisBuffer);
    
    auto clipsChanged = audioClipContainer->pull();
    auto dspClips = audioClipContainer->getConsumerObjects();
    
    for (auto clipData : dspClips)
    {
        const DspClip dspClip(getTempoProvider(), clipData);
        
        if (dspClip.getAbsolutePositionRange(audium::seconds).intersects(transportRange))
        {
            const auto transportSource = transportSourceContainer->getTransportSourceAtIndex(dspClip.dspClipData.transportSourceIndex);
            if (transportSource == nullptr)
                continue;
            
            if (not transportSource->getAudioTransportSource()->isPlaying() ||
                clipsChanged)
            {
                auto absolute = dspClip.getAbsolutePosition(audium::seconds);
                auto local = dspClip.getRegionData(audium::seconds).getStart();
                auto offset = absolute - transportPosition;
                auto position = 0.0;
                auto startSamples = 0;
                
                if (offset < 0.0)
                {
                    position = local - offset;
                    
                    startSamples = 0; // startSamples is 0
                }
                else
                {
                    position = local;
                    
                    startSamples = static_cast<int>(offset * externalSampleRate);
                    jassert(startSamples < numSamples);
                }
                
                auto duration = dspClip.getRegionData(audium::seconds).getEnd() - position;
                
                jassert(position >= 0.0 && duration >= 0.0);
                
                if ( !transportSource->getAudioTransportSource()->isPlaying()) {
                    
                    transportSource->schedulePosition(position, startSamples);
                    transportSource->scheduleDuration(duration, externalSampleRate);
                    //transportSource->getAudioTransportSource()->setGain(dspClip.dspClipData.clip_gain);
                    transportSource->getAudioTransportSource()->start();
                    playback->startVoice(transportSource);
                    
                }
//                std::cout << "transport-pos: " << transportPosition << " ";
//                std::cout << "clip-pos: " <<  absolute << " ";
//                std::cout << "offset: " << offset << " ";
//                std::cout << "file-pos: " << position << " ";
//                std::cout << "duration: " << duration << " ";
//                std::cout << "gain: " << dspClip.dspClipData.clip_gain << " ";
//                std::cout << std::endl;


            }
        }
    }
}

void PlayListScheduler::processAudio(const juce::AudioSourceChannelInfo& outputInfo)
{
    audioBusRenderer->setNumAudioBusChannels(audioTrackContainer->getNumAudioTrackChannels());
    audioBusRenderer->processAudioBlock(outputInfo);
}

juce::Range<double> PlayListScheduler::absoluteToLocalRange(juce::Range<double> absoluteRange, const PlayListItem* item, audium::TimeContextType context)
{
    auto start = absoluteToLocalPosition(absoluteRange.getStart(), item, context);
    auto end = absoluteToLocalPosition(absoluteRange.getEnd(), item, context);
    return juce::Range<double>(start, end);
}

double PlayListScheduler::absoluteToLocalPosition(double absolutePosition, const PlayListItem* item, audium::TimeContextType context)
{
    return absolutePosition - item->getAbsolutePosition(context) + item->getRegionData(context).getStart();
}

juce::Range<double> PlayListScheduler::absoluteToLocalRange(juce::Range<double> absoluteRange, std::shared_ptr<AudioSubGroup> subGroup, audium::TimeContextType context)
{
    auto start = absoluteRange.getStart() - subGroup->getAbsolutePosition(context) + subGroup->getRegionData(context).getStart();
    auto end = absoluteRange.getEnd() - subGroup->getAbsolutePosition(context) + subGroup->getRegionData(context).getStart();
    return juce::Range<double>(start, end);
}

double PlayListScheduler::getTotalLength(audium::TimeContextType context, bool addOverhead) const
{
    double totalLength = 0.0;
    
    for (auto track : audioTrackContainer->getAudioTracks())
    {
        totalLength = juce::jmax(totalLength, track->getTotalLength(context, isArrangementMode()));
    }
        
    if (addOverhead)
    {
        // add overhead to fit entire arrangement arrangement
        auto overhead = 4 * 96.0;
        if (context == audium::seconds)
        {
            overhead = tempoProvider->clocksToSeconds(overhead);
        }
        
        overhead = std::max(overhead, totalLength * 0.01);
        
        
        totalLength += overhead;
    }
    
    return totalLength;
}

void PlayListScheduler::startPlaying()
{
    if (linkEngine != nullptr)
    {
        commitPlayListData();
        linkEngine->setStartPlayingTime(getTempoProvider()->clocksToBeats(data.startPositionClocks));
        linkEngine->startPlaying();
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
    if (linkEngine != nullptr)
    {
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

void PlayListScheduler::setAbsolutePosition(double newPosition, audium::TimeContextType context)
{
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
    if (context == audium::clocks) {
        return data.startPositionClocks;
    }
    else if (context == audium::seconds) {
        return getTempoProvider()->clocksToSeconds(data.startPositionClocks);
    }
    
    jassertfalse;
    return 0.0;
}

int PlayListScheduler::getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioTrack> track)
{
    auto item = track->getPlayListContainer()->itemAtAbsolutePosition(getAbsolutePosition(audium::clocks), audium::clocks);
    if (item != nullptr)
    {
        return track->getPlayListContainer()->getPlayListItemIndex(item);
    }
    return -1;
}

void PlayListScheduler::setCurrentPositionAtPlayListItemIndex(std::shared_ptr<AudioTrack> track, int playListItemIndex)
{
    const auto item = track->getPlayListContainer()->getPlayListItem(playListItemIndex);
    if (item != nullptr)
    {
        data.startPositionClocks = item->getAbsolutePosition(audium::clocks);
    }
}

double PlayListScheduler::getPlayListItemProgress(std::shared_ptr<AudioTrack> track, int playListItemIndex) const
{
    const auto item = track->getPlayListContainer()->getPlayListItem(playListItemIndex);
    if (item != nullptr)
    {
        auto localPosition = absoluteToLocalPosition(getAbsolutePosition(audium::clocks), item.get(), audium::clocks);
        auto progress = ((localPosition - item->getRegionData(audium::clocks).getStart()) / item->getRegionData(audium::clocks).getLength());
        return progress;
    }
    return 0.0;
}

void PlayListScheduler::bounceToFile(juce::AudioFormatWriter* writer,
                                     audium::ExportAudioConfig &config,
                                     std::function<void ()> callback)
{
    // remember last position
    auto lastPosition = getAbsolutePosition(audium::seconds);
    setAbsolutePosition(config.positionSeconds, audium::seconds);

    startPlaying();
    
    jassert((int)config.sampleRate == (int)externalSampleRate);
    
    auto totalSamples   = static_cast<int64>((getTotalLength(audium::seconds) - config.positionSeconds) * externalSampleRate);
    auto iterations     = static_cast<int64>(totalSamples) / config.blockSize;
    auto remainder      = totalSamples - (iterations * config.blockSize);
    jassert(remainder < config.blockSize);
    
    auto positionBeats = TempoProvider::clocksToBeats(getAbsolutePosition(audium::clocks));
    
    AudioBuffer<float> buffer(config.numChannels, config.blockSize);
    AudioSourceChannelInfo info (&buffer, 0, config.blockSize);
    int64 samplesWritten = 0;
    for (auto i = 0; i < iterations; ++i)
    {
        const auto clocksThisBuffer = getTempoProvider()->secondsToClocks(static_cast<double>(config.blockSize) / externalSampleRate);
        const auto beatsThisBuffer = TempoProvider::clocksToBeats(clocksThisBuffer);
        
        tick(true, positionBeats, config.blockSize);
        positionBeats += beatsThisBuffer;
        
        
        buffer.clear();
        processAudio(info);

        writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
        
        samplesWritten += info.numSamples;
        
        config.progress = i / (double)iterations;
        
        if (callback)
            callback();
    }
    
    // process remaining samples
    if (remainder > 0)
    {
        buffer.clear();
        info.numSamples = static_cast<int>(remainder);
        processAudio(info);
        writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
        samplesWritten += info.numSamples;
    }
    
    jassert(samplesWritten == totalSamples);
    
    
    setAbsolutePosition(lastPosition, audium::seconds);
    stopPlaying();
    
}

void PlayListScheduler::commitPlayListData()
{
    std::cout << "PlayListScheduler::commitPlayListData" << std::endl;
    
    audioClipContainer->clear();
    
    for (auto track : audioTrackContainer->getAudioTracks()) {
        auto clips = track->getDspClipVector(isArrangementMode());
        for (auto clip : clips) {
            audioClipContainer->push_back(clip);
        }
    }

    // commit data
    audioClipContainer->commit();
        
#if 0 // print resource id and it's postion
    for (auto i = 0; i < dspClipArray.size(); i++)
    {
        if (dspClipArray[i].active)
        {
            std::cout << i << " res: " << dspClipArray[i].resourceIndex << " pos: " << dspClipArray[i].clipData.absolutePositionClocks << std::endl;
        }
    }
#endif
    
    // update channel mapping
    transportSourceContainer->applyChannelMapping();
    
}
