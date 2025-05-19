//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ResourceGroup.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Channel/AudioChannel.h"

namespace audium {

ResourceGroup::ResourceGroup(AudioTrack& audioTrack_,
                             std::shared_ptr<AudioRegionContainer> audioRegionContainer_,
                             std::shared_ptr<SelectionManager> selectionManager_) :
Selectable(selectionManager_),
audioTrack(audioTrack_),
audioRegionContainer(audioRegionContainer_)
{
    audioClip = std::shared_ptr<AudioClip> (new AudioClip(*this));
}

ResourceGroup::~ResourceGroup()
{
}

void ResourceGroup::cleanup()
{
    cleanupTransportSources();
    cleanupAudioRegions();
    cleanupAudioResources();
}

void ResourceGroup::cleanupAudioRegions()
{
    audioRegionContainer->cleanup();
}

void ResourceGroup::cleanupAudioResources()
{
    const auto audioResources = getAudioResources();
    for (auto resource : audioResources)
    {
        getAudioTrack().getAudioResourceContainer().removeAudioResource(resource);
    }
    jassert(getAudioResources().size() == 0);
}

void ResourceGroup::cleanupTransportSources()
{
    for (auto transportSource : transportSources) {
        audioTrack.getTransportSourceContainer()->removeTransportSource(transportSource);
    }
}

double ResourceGroup::getAbsolutePosition(TimeContextType context) const  {
    return audioClip->getAbsolutePosition(context);
}
void ResourceGroup::setAbsolutePosition(double position, TimeContextType context)  {
    audioClip->setAbsolutePosition(position, context);
}

juce::Range<double> ResourceGroup::getRegionData(TimeContextType context) const  {
    return audioClip->getRegionData(context);
}
void ResourceGroup::setRegionData(juce::Range<double> newRegionData, TimeContextType context)  {
    audioClip->setRegionData(newRegionData, context);
}


bool ResourceGroup::writeToJson (json& output)
{
    output["clip"] = audioClip->data;
    
    for (auto resource : getAudioResources()) {
        json j;
        resource->writeToJson(j);
        output["resources"] += j;
    }
    
    audioRegionContainer->writeToJson(output);        
    return true;
}

bool ResourceGroup::writeChannelToJson (json& output, AudioChannel* audioChannel)
{
    output["clip"] = audioClip->data;
    
    for (auto resource : getAudioResources()) {
        json j;
        if (resource->getChannelMapping().containsDestinationChannelNumber(audioChannel->getChannelNumber())) {
            resource->writeToJson(j);
            output["resources"] += j;
        }
    }
    
    audioRegionContainer->writeChannelToJson(output, audioChannel);
    
    return true;
}

std::shared_ptr<AudioResource> ResourceGroup::addAudioResourceFromUrl(juce::URL url)
{
    std::shared_ptr<AudioTrack> track       = std::dynamic_pointer_cast<AudioTrack> (getAudioTrack().getSharedPtr());
    std::shared_ptr<ResourceGroup> resourceGroup = std::dynamic_pointer_cast<ResourceGroup> (getSharedPtr());
    
    auto audioFormatReader  = track->getAudioResourceContainer().getAudioFormatReaderForUrl(url);
    auto resource           = track->getAudioResourceContainer().addAudioResource(url,
                                                                                  audioFormatReader,
                                                                                  track,
                                                                                  resourceGroup);
    if (auto transportSource = track->getAudioResourceContainer().createTransportSourceForAudioResource(resource)) {
        transportSources.push_back(transportSource);
    }
    return resource;
}

void ResourceGroup::mergeFromJson(json& input, int destinationChannel)
{
    // we use the shared_ptr
    std::shared_ptr<AudioTrack> track = std::dynamic_pointer_cast<AudioTrack> (getAudioTrack().getSharedPtr());
    std::shared_ptr<ResourceGroup> resourceGroup = std::dynamic_pointer_cast<ResourceGroup> (getSharedPtr());
    
    auto jsonResources = input["resources"];
    auto resources = getAudioResources();
    
    for (auto& jsonResource : jsonResources) {
        auto url = AudioResource::urlFromJson(jsonResource);
        AudioResource::testUrl(url);
        
        if (auto resource = addAudioResourceFromUrl(url)) {
            resource->readFromJson(jsonResource, false);
            if (destinationChannel >= 0) {
                auto sourceChannel = resource->getChannelMapping().getSourceChannel();
                resource->getChannelMapping().setOutputChannelMapping(sourceChannel, destinationChannel);
            }
        }
    }
    
    audioClip->readFromJson(input["clip"], false);
    
    audioRegionContainer->mergeFromJson(input, destinationChannel);
}

bool ResourceGroup::writeToStream (juce::OutputStream& outputStream)
{
    return Streamable::writeToStream(outputStream);
}

bool ResourceGroup::readFromJson (json& input, bool rebuild)
{
    json output;
    writeToJson(output);
    if (input == output) {
        std::cout << "skip ResourceGroup::readFromJson" << std::endl;
        return true;
    }
    
    // we use the shared_ptr
    std::shared_ptr<AudioTrack> track = std::dynamic_pointer_cast<AudioTrack> (getAudioTrack().getSharedPtr());
    std::shared_ptr<ResourceGroup> resourceGroup = std::dynamic_pointer_cast<ResourceGroup> (getSharedPtr());
    
    auto jsonResources = input["resources"];
    auto resources = getAudioResources();
    
    if (!rebuild && resources.size() != jsonResources.size()) {
        rebuild = true;
    }
    
    if (rebuild) {
        cleanup();
    }
    
    auto r = 0;
    for (auto& jsonElement : jsonResources) {
        auto url = AudioResource::urlFromJson(jsonElement);
        AudioResource::testUrl(url);
        
        std::shared_ptr<AudioResource> resource = nullptr;
        if (rebuild) {
            resource = addAudioResourceFromUrl(url);
        }
        else {
            resource = resources[r];
        }
        
        if (resource == nullptr) {
            return false;
        }
        
        resource->readFromJson(jsonElement, rebuild);
        r++;
    }
    
    audioClip->readFromJson(input["clip"], rebuild);
    
    audioRegionContainer->readFromJson(input, rebuild);
    
    return true;
}

bool ResourceGroup::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (Streamable::readFromStream(inputStream)) {
        getAudioTrack().getAudioTrackContainer().sendActionMessage(updateAll);
        return true;
    }
    return false;
}

int ResourceGroup::getSizeInUnits()
{
    return (int)getAudioResources().size() * 4;
}

std::vector<std::shared_ptr<AudioResource>> ResourceGroup::getAudioResources() const
{
    return audioTrack.getAudioResourceContainer().getAudioResourcesForSubGroup(this);
}

int ResourceGroup::getNumChannels() const
{
    return audioTrack.getNumAudioTrackChannels();
}

std::shared_ptr<AudioResource> ResourceGroup::getAudioResourceAtChannel(int channelNumber) const
{
    for (auto resource : getAudioResources())
    {
        if (resource->getChannelMapping().containsDestinationChannelNumber(channelNumber))
            return resource;
    }
    return nullptr;
}

const juce::String ResourceGroup::getName() const
{
    const auto audioResources = getAudioResources();
    if (audioResources.size() > 0)
        return audioResources[0]->getFileNameWithoutExtension();
    
    return juce::String();
}

const int ResourceGroup::getId() const
{
    auto resourceGroup = std::dynamic_pointer_cast<const ResourceGroup>(getSharedPtr());
    return audioTrack.resourceGroupContainer->getIndex(resourceGroup);
}

} // namespace audium
