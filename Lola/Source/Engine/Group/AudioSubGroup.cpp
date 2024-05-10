/*
  ==============================================================================

    AudioSubGroup.cpp
    Created: 19 Dec 2023 3:47:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioSubGroup.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioClip.h"

AudioSubGroup::AudioSubGroup(AudioGroup& audioGroup) :
    audioGroup(audioGroup)
{
    audioClip = std::shared_ptr<AudioClip> (new AudioClip(*this));
}

AudioSubGroup::~AudioSubGroup()
{
}

void AudioSubGroup::cleanup()
{
    cleanupAudioRegions();
    cleanupAudioResources();
}

void AudioSubGroup::cleanupAudioRegions()
{
    const auto audioRegions = getAudioRegions();
    for (auto region : audioRegions)
    {
        getAudioGroup().getAudioRegionContainer()->deleteAudioRegion(region);
    }
    jassert(getAudioRegions().size() == 0);
}

void AudioSubGroup::cleanupAudioResources()
{
    const auto audioResources = getAudioResources();
    for (auto resource : audioResources)
    {
        getAudioGroup().getAudioResourceContainer().removeAudioResource(resource);
    }
    jassert(getAudioResources().size() == 0);
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
    const auto group = getAudioGroup().getAudioGroupContainer().getSharedPtr(&getAudioGroup());
    const auto subGroup = getAudioGroup().getSharedPtr(this);
    
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
        auto url = jsonElement["filename"].template get<std::string>();
        std::shared_ptr<AudioResource> resource = nullptr;
        if (rebuild)
        {
            resource = getAudioGroup().getAudioResourceContainer().addAudioResource(juce::URL(url), group, subGroup);
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
    
    audioClip->data = input["clip"];

    cleanupAudioRegions();
    
    auto jsonRegions = input["regions"];

    for (auto& jsonElement : jsonRegions)
    {
        std::shared_ptr<AudioRegion> region = nullptr;
        region = getAudioGroup().getAudioRegionContainer()->createRegion(group, subGroup);
        region->data = jsonElement;
    }
    
    
    return true;
}

bool AudioSubGroup::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        getAudioGroup().getAudioGroupContainer().sendActionMessage(updateAll);
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
    return audioGroup.getAudioResourceContainer().getAudioResourcesForSubGroup(this);
}

std::vector<std::shared_ptr<AudioRegion>> AudioSubGroup::getAudioRegions() const
{
    return audioGroup.getAudioRegionContainer()->getRegionsForSubGroup(this);
}

int AudioSubGroup::getNumChannels() const
{
    return audioGroup.getNumChannels();
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


