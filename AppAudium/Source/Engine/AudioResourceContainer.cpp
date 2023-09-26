/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResourceContainer.h"
#include "AudioPlayer.h"
#include "TransportSourceProvider.h"

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
                                               std::shared_ptr<TransportSourceProvider> transportSourceProvider) :
    audioDeviceManager(audioDeviceManager),
    transportSourceProvider(transportSourceProvider)
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
        auto transportSource = transportSourceProvider->createNewTransportSource();
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
                audioResourceGroup = std::shared_ptr<AudioResourceGroup>(new AudioResourceGroup(url.getFileName().toStdString()));
            }
            else // add to first group
            {
                auto groups = getAudioResourceGroups();
                jassert(groups.size() > 0);
                audioResourceGroup = groups[0];
            }
        }
        audioResources.insert({audioResourceGroup, audioResource});
        
        inputSource.release();
        return audioResource;
    }
    
    return nullptr;
}


bool AudioResourceContainer::removeAudioResource (int atIndex)
{
    size_t idx = static_cast<size_t>(atIndex);
    if (idx < 0 || idx >= audioResources.size())
        return false;
    
    auto it = audioResources.begin();
    std::advance(it, atIndex);
    
    transportSourceProvider->removeTransportSource(it->second->getAudioTransportSource());
    audioResources.erase(it);
    
    return true;
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
        auto group = std::shared_ptr<AudioResourceGroup> (new AudioResourceGroup(groupName.toStdString()));
        
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
    for(  auto it = audioResources.begin(), end = audioResources.end(); it != end; it = audioResources.upper_bound(it->first))
    {
        result.push_back(it->first);
    }
    return result;
}
