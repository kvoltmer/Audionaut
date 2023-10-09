/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResourceContainer.h"
#include "AudioPlayer.h"
#include "TransportSourceContainer.h"
#include "AudiumTransportSource.h"
#include "Engine/ActionMessages.h"

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
                                               std::shared_ptr<TransportSourceContainer> transportSourceContainer) :
    audioDeviceManager(audioDeviceManager),
    transportSourceContainer(transportSourceContainer)
{
    formatManager.registerBasicFormats();
    thread.startThread();
}

AudioResourceContainer::~AudioResourceContainer()
{
    audioResources.clear();
}

std::shared_ptr<AudioResource> AudioResourceContainer::addAudioResource (juce::URL url, std::shared_ptr<AudioResourceGroup> group)
{
    if (auto inputSource = makeAudioInputSource (url))
    {
        /// TODO: create factory
        auto transportSource = transportSourceContainer->createNewTransportSource();
        auto audioPlayer = std::shared_ptr<AudioPlayer>(new AudioPlayer(transportSource,
                                                                        audioDeviceManager,
                                                                        inputSource.get(),
                                                                        formatManager,
                                                                        &thread));
        auto audioResource = std::shared_ptr<AudioResource>(new AudioResource(*this, url, inputSource.get(), formatManager, audioPlayer, thumbnailCache));
        
        std::shared_ptr<AudioResourceGroup> audioResourceGroup = nullptr;
        
        if (group != nullptr)
        {
            audioResourceGroup = group;
        }
        else // no group provided
        {
            // create default group
            if (audioResources.size() == 0)
            {
                audioResourceGroup = std::shared_ptr<AudioResourceGroup>(new AudioResourceGroup(*this, url.getFileName().toStdString()));
            }
            else // add to first group
            {
                auto groups = getAudioResourceGroups();
                jassert(groups.size() > 0);
                audioResourceGroup = groups[0];
            }
        }
        transportSource->setAudioResourceGroup(audioResourceGroup);
        audioResources.push_back({audioResourceGroup, audioResource});
        inputSource.release();
        
        sendActionMessage(audioResourceCreatedAction);
        
        return audioResource;
    }
    
    return nullptr;
}

bool AudioResourceContainer::removeAudioResourceGroup (int atIndex)
{
    auto group = getAudioResourceGroup(atIndex);
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
        
        if (audioResources.empty())
        {
            std::cout << "TODO: cleanup playlist and regions!" << std::endl;
        }
        return true;
    }
    
    return false;
}

int AudioResourceContainer::getNumAudioResources() const
{
    return static_cast<int>(audioResources.size());
}

int AudioResourceContainer::getNumAudioResourceGroups() const
{
    return static_cast<int>(getAudioResourceGroups().size());
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

std::shared_ptr<AudioResourceGroup> AudioResourceContainer::getAudioResourceGroup(int index) const
{
    jassert(index < getNumAudioResourceGroups());
    return getAudioResourceGroups()[index];
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
    auto groups = getAudioResourceGroups();
    outputStream.writeInt(static_cast<int>(groups.size()));
    
    for (auto & group : groups)
    {
        outputStream.writeString(juce::String(group->getName()));
        auto resources = getAudioResourcesForGroup(group);
        outputStream.writeInt(static_cast<int>(resources.size()));
        
        for (auto & resource : resources)
        {
            outputStream.writeString(resource->getUrlAsString());
        }
    }
    
    return true;
}

bool AudioResourceContainer::readFromStream (juce::InputStream& inputStream)
{
    audioResources.clear();
    
    auto numGroups = inputStream.readInt();
    for (auto g = 0; g < numGroups; g++)
    {
        auto groupName = inputStream.readString();
        auto group = std::shared_ptr<AudioResourceGroup> (new AudioResourceGroup(*this, groupName.toStdString()));
        
        auto numResources = inputStream.readInt();
        for (auto r = 0; r < numResources; r++)
        {
            auto inString = inputStream.readString();
            addAudioResource(juce::URL(inString), group);
        }
    }
    
    return true;
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForGroup(std::shared_ptr<AudioResourceGroup> group) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->first == group)
        {
            result.push_back(itr->second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AudioResourceGroup>> AudioResourceContainer::getAudioResourceGroups() const
{
    std::vector<std::shared_ptr<AudioResourceGroup>> result;
    //for(  auto it = audioResources.begin(), end = audioResources.end(); it != end; it = audioResources.upper_bound(it->first))
    for(auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        if (std::find(result.begin(), result.end(), it->first) == result.end())
        {
            result.push_back(it->first);
        }
    }
    return result;
}
