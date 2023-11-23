/*
  ==============================================================================

    AudioRegion.cpp
    Created: 30 May 2023 10:16:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioRegion.h"
#include "Engine/AudioGroup.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Provider/TempoProvider.h"

AudioRegion::~AudioRegion()
{
}

void AudioRegion::createThumbnails(std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache)
{
    
    
    // similar to AudioGroupRegionComponent
    // there is a view for each audio resource in the group
    auto resources = audioGroup->getAudioResources();
    audioThumbnails.clear();
    
    auto formatManager = audioGroup->getAudioResourceContainer().getAudioFormatManager();
    
    for (auto &resource : resources)
    {
        
        auto audioThumbnail = std::unique_ptr<juce::AudioThumbnail>(new juce::AudioThumbnail(4096*4,
                                                                                             *formatManager.get(),
                                                                                             *audioThumbnailCache.get()));
        //std::cout << "new thumbnail " << audioThumbnail.get() << " for resource " << resource.get() << std::endl;
        if (auto inputSource = std::unique_ptr<juce::URLInputSource> (new juce::URLInputSource(resource->getUrl())))
        {
            if (auto stream = rawToUniquePtr (inputSource->createInputStream()))
            {
                if (auto reader = rawToUniquePtr (formatManager->createReaderFor (std::move (stream))))
                {
//                    auto hashsource = name + resource->getUrl().getLocalFile().getFullPathName();
//                    audioThumbnail->setReader(reader.release(), hashsource.hash());
                
                    audioThumbnail->setReader(reader.release(), inputSource->hashCode());
                }
            }
        }
        
//        if (auto inputSource = AudioResourceFactory::makeAudioInputSource (resource->getUrl()))
//            audioThumbnail->setSource(inputSource.get());
        
        
        audioThumbnails.push_back({resource, std::move(audioThumbnail)});
    }
}

juce::AudioThumbnail* AudioRegion::getAudioThumbnailForResource(std::shared_ptr<AudioResource> resource) const
{
    for (auto &pair : audioThumbnails)
    {
        if (pair.first == resource)
        {
            return pair.second.get();
        }
    }
    return nullptr;
}

void AudioRegion::setRegionData(const RegionData newRegionData)
{
    setRegionDataInSeconds(tempoProvider->clocksToSeconds(newRegionData));
}

const AudioRegion::RegionData AudioRegion::getRegionData() const
{
    return tempoProvider->secondsToClocks(regionData);
}

void AudioRegion::setRegionDataInSeconds(const RegionData newRegionData)
{
    const auto resources = audioGroup->getAudioResources();
    for (auto resource : resources)
    {
        // region position must not exceed the total length of the audio file
        jassert(newRegionData.getEnd() <= resource->getLengthInSeconds());
    }
    regionData = newRegionData;
}

const AudioRegion::RegionData AudioRegion::getRegionDataInSeconds() const
{
    return regionData;
}

void AudioRegion::setRegionStart(double newStart)
{
    if (newStart <=  getRegionData().getEnd())
    {
        setRegionData(AudioRegion::RegionData(newStart, getRegionData().getEnd()));
    }
}

void AudioRegion::setRegionEnd(double newEnd)
{
    if (newEnd >=  getRegionData().getStart())
    {
        setRegionData(AudioRegion::RegionData(getRegionData().getStart(), newEnd));
    }
}


void AudioRegion::setRegionLength(double newLength)
{
    setRegionData(getRegionData().withLength(newLength));
}
