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

using namespace::std::chrono;

using namespace::juce;

void PlayListScheduler::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    externalSampleRate = sampleRate;
    bufferSize = samplesPerBlockExpected;
    tempoProvider->prepareToPlay(samplesPerBlockExpected, sampleRate);
    transportSourceContainer->prepareToPlay(samplesPerBlockExpected, sampleRate);
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
    
    auto dspClipDataVector = audioClipContainer->getDspClipDataVector();
    
    for (const auto &dspClipData : dspClipDataVector)
    {
        const DspClip dspClip(getTempoProvider(), dspClipData);
        
        if (dspClip.getAbsolutePositionRange(audium::seconds).intersects(transportRange))
        {
            const auto transportSource = transportSourceContainer->getTransportSourceAtIndex(dspClip.dspClipData.transportSourceIndex);
            if (transportSource == nullptr)
                continue;
            
            if (not transportSource->getAudioTransportSource()->isPlaying() ||
                newDataCommited.load())
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
                transportSource->schedulePosition(position, startSamples);
                transportSource->scheduleDuration(duration, externalSampleRate);
                transportSource->getAudioTransportSource()->setGain(dspClip.dspClipData.gain);
                transportSource->getAudioTransportSource()->start();
                
                std::cout << "transport-pos: " << transportPosition << " ";
                std::cout << "clip-pos: " <<  absolute << " ";
                std::cout << "offset: " << offset << " ";
                std::cout << "file-pos: " << position << " ";
                std::cout << "duration: " << duration << " ";
                std::cout << std::endl;


            }
        }
    }
    
    newDataCommited.store(false);
}

void PlayListScheduler::processAudio(const juce::AudioSourceChannelInfo& outputInfo)
{
    auto outputChannels = outputInfo.buffer->getNumChannels();
    auto totalChannels = std::max(outputInfo.buffer->getNumChannels(),
                                  audioTrackContainer->getNumAudioTrackChannels());
    juce::AudioBuffer<float> tempBuffer(totalChannels, outputInfo.numSamples);
    tempBuffer.clear();
    juce::AudioSourceChannelInfo tempBufferInfo (&tempBuffer, outputInfo.startSample, outputInfo.numSamples);
    
        
    // render the entire bus (all channels)
    transportSourceContainer->getNextAudioBlock(tempBufferInfo);
    
    // stereo or mono output
    if (outputChannels == 2 || outputChannels == 1)
    {
        for (auto c = 0; c < outputChannels; c++) {
            for (auto i = 0; i < totalChannels; i++) {
                outputInfo.buffer->addFrom(c,
                                           outputInfo.startSample,
                                           tempBuffer.getReadPointer(i),
                                           outputInfo.numSamples);
            }
        }
    }
    else // multichannel output
    {
        jassert(outputChannels == totalChannels);
        
        for (auto c = 0; c < std::min(outputChannels, totalChannels); c++) {
            outputInfo.buffer->addFrom(c,
                                       outputInfo.startSample,
                                       tempBuffer.getReadPointer(c),
                                       outputInfo.numSamples);
        }
    }
    
}

double PlayListScheduler::absoluteToLocalPosition(double absolutePosition, const PlayListItem* item, audium::TimeContextType context) const
{
    auto offset = absolutePosition - item->getAbsolutePosition(context);
    return offset + item->getRegionData(audium::clocks).getStart();
}

//void PlayListScheduler::processInArrangementMode(double absolutePosition, int numSamples)
//{
//    // iterate over track container
//    for (auto i = 0; i < audioTrackContainer->getNumItems(); i++)
//    {
//        const auto track = audioTrackContainer->getAudioTrack(i);
//        const auto playlist = track->getPlayListContainer();
//        const auto transport = track->getTransportSourceContainer();
//        const auto clocksThisBuffer = getTempoProvider()->secondsToClocks(static_cast<double>(numSamples) / externalSampleRate);
//        const auto item = playlist->itemAtAbsolutePosition(absolutePosition + clocksThisBuffer, audium::clocks);
//        
//        if (item != playlist->currentPlayListItem)
//        {
//            playlist->currentPlayListItem = item;
//            
//            if (item == nullptr)
//            {
//                transport->stopPlaying();
//            }
//            else
//            {
//                // set the position as accurate as possible
//                const auto startPosition = playlist->currentPlayListItem->getAbsolutePosition(audium::clocks);
//                const auto localPosition = absoluteToLocalPosition(startPosition, playlist->currentPlayListItem, audium::clocks);
//                
//                const auto diff = startPosition - absolutePosition;
//                const auto startSamples = static_cast<int>(getTempoProvider()->clocksToSeconds(diff) * externalSampleRate);
//                
//                if (diff < 0.0)
//                {
//                    transport->setLocalPosition(getTempoProvider()->clocksToSeconds(localPosition - diff), 0);
//                }
//                else
//                {
//                    jassert(startSamples < numSamples);
//                    transport->setLocalPosition(getTempoProvider()->clocksToSeconds(localPosition), startSamples);
//                }
//                
//                std::cout << "pos " << absolutePosition << " start " <<  startPosition << " diff " << diff << " samples " << startSamples << std::endl;
//                
//                if (not transport->isPlaying())
//                {
//                    transport->startPlaying();
//                }
//            }
//        }
//    }
//}
//
//void PlayListScheduler::processInEditMode(double absolutePosition, int numSamples)
//{
//    // convert to seconds
//    const auto absolutePositionInSeconds = tempoProvider->clocksToSeconds(absolutePosition);
//    const auto secondsThisBuffer = static_cast<double>(numSamples) / externalSampleRate;
//
//    const auto resources = audioResourceContainer->resourcesAtAbsolutePosition(absolutePositionInSeconds + secondsThisBuffer);
//    for (auto resource : resources)
//    {
//        if (not resource->getAudioTransportSource()->isPlaying())
//        {
//            const auto startPosition = resource->getAudioSubGroup()->getAudioClip()->getAbsolutePosition(audium::seconds);
//            const auto localPosition = resource->getAudioSubGroup()->getAudioClip()->getRegionData(audium::seconds).getStart();
//            const auto diff = startPosition - absolutePositionInSeconds;
//
//            if (diff < 0.0)
//            {
//                resource->getAudioTransportSource()->schedulePosition(localPosition - diff, 0);
//            }
//            else
//            {
//                const auto startSamples = static_cast<int>(diff * externalSampleRate);
//                jassert(startSamples < numSamples);
//                resource->getAudioTransportSource()->schedulePosition(localPosition, startSamples);
//            }
//            
//            std::cout << "absolute pos: " << absolutePositionInSeconds << " start " <<  startPosition << " diff " << diff << std::endl;
//
//            resource->getAudioTransportSource()->scheduleDuration(resource->getAudioSubGroup()->getAudioClip()->getRegionData(audium::seconds).getLength(), externalSampleRate);
//        }
//    }
//}


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
    
    if (linkEngine != nullptr)
    {
        linkEngine->stopPlaying();
    }
    
    transportSourceContainer->stopPlaying();
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
    const auto clocks = isPlaying() ? data.transportPositionClocks : data.startPositionClocks;
    if (context == audium::clocks)
    {
        return clocks;
    }
    else if (context == audium::seconds)
    {
        return getTempoProvider()->clocksToSeconds(clocks);
    }
    
    jassertfalse;
    return 0.0;
}

void PlayListScheduler::setAbsolutePosition(double newPosition, audium::TimeContextType context)
{
    if (linkEngine != nullptr)
    {
        if (context == audium::clocks)
        {
            data.startPositionClocks = newPosition;
        }
        else if (context == audium::seconds)
        {
            data.startPositionClocks = getTempoProvider()->secondsToClocks(newPosition);
        }
    }
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
    
    DspClipArray<> dspClipArray;
        
    int count = 0;
    
    
    for (const auto &track : audioTrackContainer->getAudioTracks())
    {
        auto dspClipData = track->getDspClipVector(isArrangementMode());
        
        for (const auto &clip : dspClipData)
        {
            if (count < dspClipArray.size())
            {
                dspClipArray[count++] = clip;
            }
            else
            {
                std::cout << "tDspClipArray overflow " << count << std::endl;
                return;
            }
        }
    }

    // commit data as atomic operation
    audioClipContainer->atomicDspClipArray.store(dspClipArray);
    newDataCommited.store(true);
    
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
