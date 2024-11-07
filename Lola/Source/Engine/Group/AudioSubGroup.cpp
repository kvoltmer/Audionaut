/*
  ==============================================================================

    AudioSubGroup.cpp
    Created: 19 Dec 2023 3:47:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioSubGroup.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/AudioSources/TransportSourceContainer.h"

AudioSubGroup::AudioSubGroup(AudioTrack& audioTrack,
                             std::shared_ptr<audium::SelectionManager> selectionManager) :
    audium::Selectable(selectionManager),
    audioTrack(audioTrack)
{
    audioClip = std::shared_ptr<AudioClip> (new AudioClip(*this));
}

AudioSubGroup::~AudioSubGroup()
{
}

void AudioSubGroup::cleanup()
{
    cleanupTransportSources();
    cleanupAudioRegions();
    cleanupAudioResources();
}

void AudioSubGroup::cleanupAudioRegions()
{
    const auto audioRegions = getAudioRegions();
    for (auto region : audioRegions)
    {
        getAudioTrack().getAudioRegionContainer()->deleteAudioRegion(region);
    }
    jassert(getAudioRegions().size() == 0);
}

void AudioSubGroup::cleanupAudioResources()
{
    const auto audioResources = getAudioResources();
    for (auto resource : audioResources)
    {
        getAudioTrack().getAudioResourceContainer().removeAudioResource(resource);
    }
    jassert(getAudioResources().size() == 0);
}

void AudioSubGroup::cleanupTransportSources()
{
    for (auto transportSource : transportSources)
    {
        audioTrack.getTransportSourceContainer()->removeTransportSource(transportSource);
    }
}

bool AudioSubGroup::writeToJson (json& output)
{
    output["clip"] = audioClip->data;

    for (auto resource : getAudioResources())
    {
        json j;
        resource->writeToJson(j);
        output["resources"] += j;
    }
    
    for (auto region : getAudioRegions())
    {
        output["regions"] += region->data;
    }
        
    return true;
}

bool AudioSubGroup::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioSubGroup::readFromJson (json& input, bool rebuild)
{
    json output;
    writeToJson(output);
    if (input == output)
    {
        std::cout << "skip AudioSubGroup::readFromJson" << std::endl;
        return true;
    }
    
    // we use the shared_ptr
    std::shared_ptr<AudioTrack> track = std::dynamic_pointer_cast<AudioTrack> (getAudioTrack().getSharedPtr());
    std::shared_ptr<AudioSubGroup> subGroup = std::dynamic_pointer_cast<AudioSubGroup> (getSharedPtr());
    
    if (rebuild)
        cleanup();
    
    auto jsonResources = input["resources"];
    auto resources = getAudioResources();
    if (!rebuild)
    {
        jassert(resources.size() == jsonResources.size());
    }
    
    auto r = 0;
    for (auto& jsonElement : jsonResources)
    {
        auto url = AudioResource::urlFromJson(jsonElement);
        std::shared_ptr<AudioResource> resource = nullptr;
        if (rebuild)
        {
            resource = getAudioTrack().getAudioResourceContainer().addAudioResource(url, track, subGroup);
            if (resource != nullptr)
            {
                if (auto transportSource = getAudioTrack().getAudioResourceContainer().createTransportSourceForAudioResource(resource))
                {
                    transportSources.push_back(transportSource);
                }
                else
                {
                    return false;
                }
            }
        }
        else
        {
            resource = resources[r];
        }
        
        if (resource == nullptr)
        {
            jassertfalse;
            return false;
        }
        
        resource->readFromJson(jsonElement, rebuild);
        
        r++;
    }
    
    audioClip->readFromJson(input["clip"], rebuild);

    if (rebuild)
        cleanupAudioRegions();
    
    auto jsonRegions = input["regions"];

    
    for (auto& jsonElement : jsonRegions)
    {
        AudioRegionData data = jsonElement;
        
        std::shared_ptr<AudioRegion> region = nullptr;
        if (rebuild)
        {
            region = getAudioTrack().getAudioRegionContainer()->createRegion(track, subGroup);
        }
        else
        {
            region = getAudioTrack().getAudioRegionContainer()->getRegion(data.id);
        }
        jassert(region);
        auto old_id = region->data.id;
        // assign data
        region->data = data;
        // keep old id
        region->data.id = old_id;
    }
    
    return true;
}

bool AudioSubGroup::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        getAudioTrack().getAudioTrackContainer().sendActionMessage(updateAll);
        return true;
    }
    return false;
}

int AudioSubGroup::getSizeInUnits()
{
    return (int)getAudioResources().size() * 4;
}

std::vector<std::shared_ptr<AudioResource>> AudioSubGroup::getAudioResources() const
{
    return audioTrack.getAudioResourceContainer().getAudioResourcesForSubGroup(this);
}

std::vector<std::shared_ptr<AudioRegion>> AudioSubGroup::getAudioRegions() const
{
    return audioTrack.getAudioRegionContainer()->getRegionsForSubGroup(this);
}

int AudioSubGroup::getNumChannels() const
{
    return audioTrack.getNumAudioTrackChannels();
}

std::shared_ptr<AudioResource> AudioSubGroup::getChannel(int rowNumber) const
{
    for (auto resource : getAudioResources())
    {
        if (resource->containsChannelNumber(rowNumber))
            return resource;
    }
    return nullptr;
}
