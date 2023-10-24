/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResourceContainer.h"
#include "AudioGroupContainer.h"
#include "AudioPlayer.h"
#include "TransportSourceContainer.h"
#include "AudiumTransportSource.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudiumEngine.h"

static std::unique_ptr<juce::InputSource> makeAudioInputSource (const juce::URL& url)
{
   #if JUCE_ANDROID
    if (auto doc = AndroidDocument::fromDocument (url))
        return std::make_unique<AndroidDocumentInputSource> (doc);
   #endif

   #if ! JUCE_IOS
    if (url.isLocalFile())
        return std::make_unique<juce::FileInputSource> (url.getLocalFile());
   #endif

    return std::make_unique<juce::URLInputSource> (url);
}

AudioResourceContainer::AudioResourceContainer(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                                               std::shared_ptr<AudioGroupContainer> audioGroupContainer) :
    audioDeviceManager(audioDeviceManager),
    audioGroupContainer(audioGroupContainer)
{
    formatManager.registerBasicFormats();
    thread.startThread();
}

AudioResourceContainer::~AudioResourceContainer()
{
    audioResources.clear();
}

std::shared_ptr<AudioResource> AudioResourceContainer::addAudioResource (juce::URL url,
                                                                         const AudiumEngine& engine,
                                                                         std::shared_ptr<AudioGroup> group)
{
    if (group != nullptr)
    {
        if (auto inputSource = makeAudioInputSource (url))
        {
            /// TODO: create factory
            auto transportSource = group->getTransportSourceContainer()->createNewTransportSource();
            auto audioPlayer = std::shared_ptr<AudioPlayer>(new AudioPlayer(transportSource,
                                                                            audioDeviceManager,
                                                                            inputSource.get(),
                                                                            formatManager,
                                                                            &thread));
            auto audioResource = std::shared_ptr<AudioResource>(new AudioResource(*this, url, inputSource.get(), formatManager, audioPlayer, thumbnailCache));
            
            std::shared_ptr<AudioGroup> audioGroup = nullptr;
            
            if (group != nullptr)
            {
                audioGroup = group;
            }
            else // no group provided
            {
                // create default group
                if (audioGroupContainer->getNumItems() == 0)
                {
                    audioGroup = audioGroupContainer->createNewAudioGroup(*this, *engine.getAudioRegionContainer(), url.getFileName().toStdString());
                }
                else // add to first group
                {
                    audioGroup = audioGroupContainer->getAudioGroup(0);
                }
            }
            
            audioResources.push_back({audioGroup, audioResource});
            inputSource.release();
            
            sendActionMessage(audioResourceCreatedAction);
            
            return audioResource;
        }
    }
    
    return nullptr;
}

void AudioResourceContainer::removeAudioResource(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<AudioResource> resource)
{
    for (auto it = audioResources.begin(); it != audioResources.end();)
    {
        if ((*it).second == resource)
        {
            auto region = (*it).second;
            region->getAudioPlayer()->stopAudio();
            
            auto group = (*it).first;
            group->getTransportSourceContainer()->removeTransportSource(region->getAudioTransportSource());
            
            // any resource left in group?
            if (getAudioResourcesForGroup(group.get()).size() == 0)
            {
                audioGroupContainer->removeAudioGroup(engine, group);
            }
            
            audioResources.erase(it);
            break;
        }
        ++it;
    }
    
    sendActionMessage(audioResourceRemovedAction);
}

void AudioResourceContainer::removeAudioResourcesForGroup (std::shared_ptr<AudioGroup> group)
{
    jassert(group != nullptr);
    if (group != nullptr)
    {        
        for (auto it = audioResources.begin(); it != audioResources.end();)
        {
            if ((*it).first == group)
            {
                audioResources.erase(it++);
            }
            else
            {
                ++it;
            }
        }
    }
}

int AudioResourceContainer::getNumAudioResources() const
{
    return static_cast<int>(audioResources.size());
}

int AudioResourceContainer::getNumAudioGroups() const
{
    jassert(static_cast<int>(getAudioGroups().size()) == audioGroupContainer->getNumItems());
    return audioGroupContainer->getNumItems();
}

std::shared_ptr<AudioResource> AudioResourceContainer::getAudioResource(int index) const
{
    if (index < getNumAudioResources())
    {
        auto it = audioResources.begin();
        std::advance(it, index);
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<AudioGroup> AudioResourceContainer::getAudioGroup(int index) const
{
    jassert(index < getNumAudioGroups());
    return getAudioGroups()[index];
}


double AudioResourceContainer::getTotalLengthMax() const
{
    double length = 0;// 420;
    
    for (auto & element : audioResources)
    {
        length = std::max(length, element.second->getThumbnail().getTotalLength());
    }
    return length;
}

bool AudioResourceContainer::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(static_cast<int>(audioResources.size()));
        
    for (auto resource : audioResources)
    {
        // group id and name
        outputStream.writeInt(resource.first->getId());
        outputStream.writeString(resource.first->getName());
        
        // url of the resource
        outputStream.writeString(resource.second->getUrlAsString());
        outputStream.writeFloat(resource.second->getAudioPlayer()->getGain());
    }
    return true;
}

bool AudioResourceContainer::readFromStream (juce::InputStream& inputStream, const AudiumEngine& engine)
{
    jassert(audioResources.empty());
    
    auto numResources = inputStream.readInt();
    for (auto i = 0; i < numResources; i++)
    {
        // group id and name
        auto groupId = inputStream.readInt();
        auto groupName = inputStream.readString();
        auto group = audioGroupContainer->getAudioGroupById(groupId);
        if (group != nullptr && group->getName() == groupName)
        {
            jassert(group && group->getName() == groupName);
            
            // url of the resource
            auto inString = inputStream.readString();
            auto resource = addAudioResource(juce::URL(inString), engine, group);
            auto gain = inputStream.readFloat();
            resource->getAudioPlayer()->setGain(gain);
        }
        else
        {
            return false;
        }
    }
    return true;
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForGroup(AudioGroup *group) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->first.get() == group)
        {
            result.push_back(itr->second);
        }
    }
    return result;
}

std::shared_ptr<AudioGroup> AudioResourceContainer::getAudioGroupForResource(std::shared_ptr<AudioResource> resource) const
{
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->second == resource)
        {
            return itr->first;
        }
    }
    return nullptr;
    
}

std::vector<std::shared_ptr<AudioGroup>> AudioResourceContainer::getAudioGroups() const
{
    std::vector<std::shared_ptr<AudioGroup>> result;
    for(auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        if (std::find(result.begin(), result.end(), it->first) == result.end())
        {
            result.push_back(it->first);
        }
    }
    return result;
}

std::shared_ptr<AudioGroup> AudioResourceContainer::getDefaultGroup() const
{
    auto groups = getAudioGroups();
    if (groups.size() > 0)
    {
        return groups[0];
    }
    return nullptr;
}

int AudioResourceContainer::getNumChannels() const
{
    auto count = 0;

    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        auto audioResources = getAudioResourcesForGroup(group.get());
        for (auto resource : audioResources)
        {
            count += resource->getNumChannels();
        }
        
    }
    return count;
}

std::shared_ptr<AudioResource> AudioResourceContainer::getChannel(int index) const
{
    auto count = 0;
    
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        auto audioResources = getAudioResourcesForGroup(group.get());
        for (auto resource : audioResources)
        {
            for (auto c = 0; c < resource->getNumChannels(); c++)
            {
                if (count == index)
                {
                    return resource;
                }
                count++;
            }
        }
    }
    jassertfalse;
    return nullptr;
}
