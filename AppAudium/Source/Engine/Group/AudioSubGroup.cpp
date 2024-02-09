/*
  ==============================================================================

    AudioSubGroup.cpp
    Created: 19 Dec 2023 3:47:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioSubGroup.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/AudioRegionContainer.h"
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

bool AudioSubGroup::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(subGroupId);

    audioClip->writeToStream(outputStream);

    // Resources
    const auto audioResources = getAudioResources();
    outputStream.writeInt(static_cast<int>(audioResources.size()));
        
    for (auto resource : audioResources)
    {
        // group id and name
        outputStream.writeInt(getAudioGroup().getId());
        outputStream.writeString(getAudioGroup().getName());
        
        // write audio resource data
        resource->writeToStream(outputStream);
        
        jassert(resource->getAudioSubGroup()->getId() == subGroupId);
    }
    
    // Regions
    const auto audioRegions = getAudioRegions();
    outputStream.writeInt(static_cast<int>(audioRegions.size()));
    for (auto region : audioRegions)
    {
        region->writeToStream(outputStream);
    }
        
    return true;
}

bool AudioSubGroup::readFromStream (juce::InputStream& inputStream)
{
    cleanup();
    
    // we use the shared_ptr
    const auto group = getAudioGroup().getAudioGroupContainer().getAudioGroupById(getAudioGroup().getId());
    const auto subGroup = getAudioGroup().getAudioSubGroupById(getId());
    if (group == nullptr || subGroup == nullptr)
    {
        jassertfalse;
        return false;
    }
    
    subGroupId              = inputStream.readInt();

    audioClip->readFromStream(inputStream);
    
    // Resources
    auto numResources = inputStream.readInt();
    for (auto i = 0; i < numResources; i++)
    {
        // group id and name
        auto groupId =      inputStream.readInt();
        auto groupName =    inputStream.readString();
        jassert(groupId == getAudioGroup().getId());
        jassert(groupName == getAudioGroup().getName());
        
        // url of the resource
        auto streamPos = inputStream.getPosition();
        auto url = inputStream.readString();
        auto resource = getAudioGroup().getAudioResourceContainer().addAudioResource(juce::URL(url), group, subGroup);
        if (resource == nullptr)
            return false;
            
        inputStream.setPosition(streamPos);
        
        // audio resource
        resource->readFromStream(inputStream);
        auto channelsNeeded = resource->getChannelPosition() + resource->getNumChannels();
        getAudioGroup().ensureNumChannels(channelsNeeded);
    }
    

    // Regions
    auto numRegions = inputStream.readInt();
    for (auto i = 0; i < numRegions; i++)
    {
        auto region = getAudioGroup().getAudioRegionContainer().createRegion(group, subGroup);
        region->readFromStream(inputStream);
    }
    
    audioClip->validateData();
    
    getAudioGroup().getAudioGroupContainer().sendActionMessage(updateAll);
    return true;
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


