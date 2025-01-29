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
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Channel/AudioChannel.h"

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

double AudioSubGroup::getAbsolutePosition(audium::TimeContextType context) const  {
    return audioClip->getAbsolutePosition(context);
}
void AudioSubGroup::setAbsolutePosition(double position, audium::TimeContextType context)  {
    audioClip->setAbsolutePosition(position, context);
}

juce::Range<double> AudioSubGroup::getRegionData(audium::TimeContextType context) const  {
    return audioClip->getRegionData(context);
}
void AudioSubGroup::setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context)  {
    audioClip->setRegionData(newRegionData, context);
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
        json r;
        region->writeToJson(r);
        output["regions"] += r;
    }
        
    return true;
}

bool AudioSubGroup::writeChannelToJson (json& output, AudioChannel* audioChannel)
{
    output["clip"] = audioClip->data;

    for (auto resource : getAudioResources())
    {
        json j;
        
        if (resource->getChannelMapping().containsDestinationChannelNumber(audioChannel->getChannelNumber())) {
            resource->writeToJson(j);
            output["resources"] += j;
        }
    }
    
    for (auto region : getAudioRegions())
    {
        output["regions"] += region->data;
    }
    return true;
}

void AudioSubGroup::mergeFromJson(json& input, int destinationChannel)
{
    // we use the shared_ptr
    std::shared_ptr<AudioTrack> track = std::dynamic_pointer_cast<AudioTrack> (getAudioTrack().getSharedPtr());
    std::shared_ptr<AudioSubGroup> subGroup = std::dynamic_pointer_cast<AudioSubGroup> (getSharedPtr());
    
    auto jsonResources = input["resources"];
    auto resources = getAudioResources();
    
    for (auto& jsonResource : jsonResources) {
        auto url = AudioResource::urlFromJson(jsonResource);
        AudioResource::testUrl(url);
        
        std::shared_ptr<AudioResource> resource = nullptr;

        auto audioFormatReader = getAudioTrack().getAudioResourceContainer().getAudioFormatReaderForUrl(url);
        resource = getAudioTrack().getAudioResourceContainer().addAudioResource(url,
                                                                                    audioFormatReader,
                                                                                    track,
                                                                                    subGroup);
        if (auto transportSource = getAudioTrack().getAudioResourceContainer().createTransportSourceForAudioResource(resource)) {
            transportSources.push_back(transportSource);
        }
        
        resource->readFromJson(jsonResource, false);
        if (destinationChannel >= 0) {
            auto sourceChannel = resource->getChannelMapping().getSourceChannel();
            resource->getChannelMapping().setOutputChannelMapping(sourceChannel, destinationChannel);
        }
    }
    
    audioClip->readFromJson(input["clip"], false);

    auto jsonRegions = input["regions"];
    
    for (auto& jsonRegion : jsonRegions) {
        AudioRegionData data = jsonRegion;
        
        auto region = track->getAudioRegionContainer()->getRegionWithData(data);
        
        if (region == nullptr)
            region = getAudioTrack().getAudioRegionContainer()->createRegion(track, subGroup);
        
        jassert(region);
        auto old_id = region->data.region_id;
        // assign data
        region->data = data;
        // keep old id
        region->data.region_id = old_id;
    }
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
        AudioResource::testUrl(url);
        
        std::shared_ptr<AudioResource> resource = nullptr;
        if (rebuild)
        {
            auto audioFormatReader = getAudioTrack().getAudioResourceContainer().getAudioFormatReaderForUrl(url);
            resource = getAudioTrack().getAudioResourceContainer().addAudioResource(url,
                                                                                    audioFormatReader,
                                                                                    track,
                                                                                    subGroup);
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
            region = getAudioTrack().getAudioRegionContainer()->getRegion(data.region_id);
        }
        jassert(region);
        auto old_id = region->data.region_id;
        // assign data
        region->data = data;
        // keep old id
        region->data.region_id = old_id;
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
        if (resource->getChannelMapping().containsSourceChannelNumber(rowNumber))
            return resource;
    }
    return nullptr;
}

const juce::String AudioSubGroup::getName() const
{
    const auto audioResources = getAudioResources();
    if (audioResources.size() > 0)
        return audioResources[0]->getFileNameWithoutExtension();
    
    return juce::String();
}
