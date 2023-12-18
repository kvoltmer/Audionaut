/*
  ==============================================================================

    AudioGroupContainer.cpp
    Created: 10 Oct 2023 12:12:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioGroupContainer.h"
#include "Engine/AudioGroup.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/Factory/AudioGroupFactory.h"

AudioGroupContainer::~AudioGroupContainer()
{
    jassert(audioGroups.empty());
}

void AudioGroupContainer::cleanup()
{
    for (auto group : audioGroups)
    {
        group->cleanup();
    }
    audioGroups.clear();
    
    nextId = 0;
}

bool AudioGroupContainer::groupIdExists(const int groupId) const
{
    for (auto group : audioGroups)
    {
        if (group->getId() == groupId)
            return true;
    }
    return false;
}

std::shared_ptr<AudioGroup> AudioGroupContainer::getAudioGroup(int index) const
{
    if (index >= 0 && index < audioGroups.size())
    {
        return audioGroups[index];
    }
    jassertfalse;
    return nullptr;
}

std::shared_ptr<AudioGroup> AudioGroupContainer::getAudioGroupById(int groupId) const
{
    for (auto group : audioGroups)
    {
        if (group->getId() == groupId)
            return group;
    }
    jassertfalse;
    return nullptr;
}



std::shared_ptr<AudioGroup> AudioGroupContainer::createNewAudioGroup(const AudioResourceContainer &audioResourceContainer,
                                                                     const AudioRegionContainer &audioRegionContainer,
                                                                     std::string nameString,
                                                                     int groupId)
{
    groupId = (groupId < 0) ? getNextId() : groupId;
    jassert( !groupIdExists(groupId) );
    
    auto audioGroup = AudioGroupFactory::createAudioGroup(audioResourceContainer, audioRegionContainer);
    
    audioGroup->setName(nameString);
    audioGroup->setId(groupId);
    
    audioGroups.push_back(audioGroup);
    std::cout << "audio group created with id = " << groupId << std::endl;
    sendActionMessage(audioGroupCreatedAction);
    return audioGroup;
}

bool AudioGroupContainer::removeAudioGroup(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<AudioGroup> group)
{
    engine->getAudioRegionContainer()->removeAudioRegionsForGroup(group);
    engine->getAudioResourceContainer()->removeAudioResourcesForGroup(group);
    
    auto it = std::find(audioGroups.begin(), audioGroups.end(), group);
    if (it != audioGroups.end())
    {
        group->cleanup();
        audioGroups.erase(it);
        sendActionMessage(audioGroupDeletedAction);
        return true;
    }
    return false;
}

bool AudioGroupContainer::writeToStream (juce::OutputStream& outputStream)
{
    
    outputStream.writeInt(static_cast<int>(audioGroups.size()));
    
    for (auto & group : audioGroups)
    {
        group->writeToStream(outputStream);
    }
    
    return true;
}

bool AudioGroupContainer::readFromStream (juce::InputStream& inputStream,
                                          const AudioResourceContainer &audioResourceContainer,
                                          const AudioRegionContainer &audioRegionContainer)
{
    jassert(audioGroups.size() == 0);
    jassert(nextId == 0);
    
    auto numGroups = inputStream.readInt();
        
    for (auto g = 0; g < numGroups; g++)
    {
        auto audioGroup = AudioGroupFactory::createAudioGroup(audioResourceContainer, audioRegionContainer);
        audioGroup->readFromStream(inputStream);
        audioGroups.push_back(audioGroup);
        nextId = juce::jmax(nextId, audioGroup->getId());
    }
    
    return true;
}

std::shared_ptr<AudioGroup> AudioGroupContainer::getDefaultGroup() const
{
    if (audioGroups.size() > 0)
    {
        return audioGroups[0];
    }
    return nullptr;
}



