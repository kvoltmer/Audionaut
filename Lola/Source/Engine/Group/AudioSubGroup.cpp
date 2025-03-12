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

AudioSubGroup::AudioSubGroup(AudioTrack& audioTrack_,
                             std::shared_ptr<AudioRegionContainer> audioRegionContainer_,
                             std::shared_ptr<audium::SelectionManager> selectionManager_) :
    audium::Selectable(selectionManager_),
    audioTrack(audioTrack_),
    audioRegionContainer(audioRegionContainer_)
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
    audioRegionContainer->cleanup();
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
    
    audioRegionContainer->writeToJson(output);        
    return true;
}

bool AudioSubGroup::writeChannelToJson (json& output, AudioChannel* audioChannel)
{
    output["clip"] = audioClip->data;

    for (auto resource : getAudioResources()) {
        json j;
        if (resource->getChannelMapping().containsDestinationChannelNumber(audioChannel->getChannelNumber())) {
            resource->writeToJson(j);
            output["resources"] += j;
        }
    }
    
    audioRegionContainer->writeToJson(output);
    
    return true;
}

std::shared_ptr<AudioResource> AudioSubGroup::addAudioResourceFromUrl(const juce::URL url)
{
    std::shared_ptr<AudioTrack> track       = std::dynamic_pointer_cast<AudioTrack> (getAudioTrack().getSharedPtr());
    std::shared_ptr<AudioSubGroup> subGroup = std::dynamic_pointer_cast<AudioSubGroup> (getSharedPtr());
    
    auto audioFormatReader  = track->getAudioResourceContainer().getAudioFormatReaderForUrl(url);
    auto resource           = track->getAudioResourceContainer().addAudioResource(url,
                                                                                  audioFormatReader,
                                                                                  track,
                                                                                  subGroup);
    if (auto transportSource = track->getAudioResourceContainer().createTransportSourceForAudioResource(resource)) {
        transportSources.push_back(transportSource);
    }
    return resource;
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
        
        if (auto resource = addAudioResourceFromUrl(url)) {
            resource->readFromJson(jsonResource, false);
            if (destinationChannel >= 0) {
                auto sourceChannel = resource->getChannelMapping().getSourceChannel();
                resource->getChannelMapping().setOutputChannelMapping(sourceChannel, destinationChannel);
            }
        }
    }
    
    audioClip->readFromJson(input["clip"], false);

    audioRegionContainer->mergeFromJson(input);
}

bool AudioSubGroup::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioSubGroup::readFromJson (json& input, bool rebuild)
{
    json output;
    writeToJson(output);
    if (input == output) {
        std::cout << "skip AudioSubGroup::readFromJson" << std::endl;
        return true;
    }
    
    // we use the shared_ptr
    std::shared_ptr<AudioTrack> track = std::dynamic_pointer_cast<AudioTrack> (getAudioTrack().getSharedPtr());
    std::shared_ptr<AudioSubGroup> subGroup = std::dynamic_pointer_cast<AudioSubGroup> (getSharedPtr());
    
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

bool AudioSubGroup::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream)) {
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

int AudioSubGroup::getNumChannels() const
{
    return audioTrack.getNumAudioTrackChannels();
}

std::shared_ptr<AudioResource> AudioSubGroup::getAudioResourceAtChannel(int channelNumber) const
{
    for (auto resource : getAudioResources())
    {
        if (resource->getChannelMapping().containsDestinationChannelNumber(channelNumber))
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

const int AudioSubGroup::getId() const
{
    return audioTrack.audioSubGroupContainer->getIndex(std::dynamic_pointer_cast<const AudioSubGroup>(getSharedPtr()));
}
