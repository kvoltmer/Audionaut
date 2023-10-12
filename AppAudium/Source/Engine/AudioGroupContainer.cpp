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

AudioGroupContainer::~AudioGroupContainer()
{
    jassert(audioGroups.empty());
}

void AudioGroupContainer::cleanup()
{
    for (auto group : audioGroups)
    {
        group->getPlayListContainer()->cleanup();
    }
    audioGroups.clear();
}

std::shared_ptr<AudioGroup> AudioGroupContainer::createNewAudioGroup(const AudioResourceContainer &audioResourceContainer,
                                                                     const AudioRegionContainer &audioRegionContainer,
                                                                     std::string nameString)
{
    auto playListContainer = std::shared_ptr<PlayListContainer> (new PlayListContainer(audioRegionContainer));
    auto audioGroup = std::shared_ptr<AudioGroup>(new AudioGroup(audioResourceContainer, playListContainer, nameString));
    audioGroups.push_back(audioGroup);
    sendActionMessage(audioGroupCreatedAction);
    return audioGroup;
}

bool AudioGroupContainer::removeAudioGroup(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<AudioGroup> group)
{
    
    engine->getAudioResourceContainer()->removeAudioResourcesForGroup(group);
    engine->getAudioRegionContainer()->removeAudioRegionsForGroup(group);
    
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
        outputStream.writeString(juce::String(group->getName()));
        group->getPlayListContainer()->writeToStream(outputStream);
    }
    
    return true;
}

bool AudioGroupContainer::readFromStream (juce::InputStream& inputStream)
{
    auto numGroups = inputStream.readInt();
    jassert((int)audioGroups.size() == numGroups);
    for (auto g = 0; g < numGroups; g++)
    {
        auto groupName = inputStream.readString();
        if (g < audioGroups.size())
        {
            jassert(audioGroups[g]);
            audioGroups[g]->getPlayListContainer()->readFromStream(inputStream);
        }
    }
    
    return true;
}



