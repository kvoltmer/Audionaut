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

AudioSubGroup::AudioSubGroup(AudioGroup& audioGroup, int subGroupId) :
    audioGroup(audioGroup),
    subGroupId(subGroupId)
{
    audioClip = std::shared_ptr<AudioClip> (new AudioClip(*this));
}

AudioSubGroup::~AudioSubGroup()
{
//    jassert(audioGroup.getAudioResourceContainer().getAudioResourcesForSubGroup(this).size() == 0);
//    jassert(audioGroup.getAudioRegionContainer().getRegionsForSubGroup(this).size() == 0);
}

void AudioSubGroup::cleanup()
{
    const auto audioRegions = getAudioRegions();
    for (auto region : audioRegions)
    {
        getAudioGroup().getAudioRegionContainer().deleteAudioRegion(region);
    }
    jassert(getAudioRegions().size() == 0);
    
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

bool AudioSubGroup::readFromJson (json& input)
{
    // we use the shared_ptr
    const auto group = getAudioGroup().getAudioGroupContainer().getAudioGroupById(getAudioGroup().getId());
    const auto subGroup = getAudioGroup().getAudioSubGroupById(getId());
    if (group == nullptr || subGroup == nullptr)
    {
        jassertfalse;
        return false;
    }
    
    cleanup();
    
    audioClip->data = input["clip"];
    
    auto jsonResources = input["resources"];
    for (auto& jsonElement : jsonResources)
    {
        auto url = jsonElement["filename"].template get<std::string>();

        auto resource = getAudioGroup().getAudioResourceContainer().addAudioResource(juce::URL(url), group, subGroup);
        if (resource == nullptr)
            return false;
        
        resource->readFromJson(jsonElement);
    }
    
    
    auto jsonRegions = input["regions"];
    for (auto& jsonElement : jsonRegions)
    {
        auto region = getAudioGroup().getAudioRegionContainer().createRegion(group, subGroup);
        region->data = jsonElement;
    }
    
    audioClip->validateData();
    return true;
}

bool AudioSubGroup::readFromStream (juce::InputStream& inputStream)
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
    return audioGroup.getAudioRegionContainer().getRegionsForSubGroup(this);
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


