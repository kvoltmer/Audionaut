/*
  ==============================================================================

    AudioTrack.cpp
    Created: 28 Sep 2023 1:33:20pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Resource/ChannelMapping.h"

AudioTrack::~AudioTrack()
{
    cleanup();
}

void AudioTrack::cleanup()
{
    audioSubGroupContainer->cleanup();
    audioChannelContainer->cleanup();
    playListContainer->playListItems.cleanup();
}

void AudioTrack::setAudioTrackName(const juce::String newName)
{
    name = newName.toStdString();
}

std::vector<std::shared_ptr<AudioResource>> AudioTrack::getAudioResources() const
{
    return audioResourceContainer.getAudioResourcesForTrack(const_cast<AudioTrack*>(this));
}

std::vector<std::shared_ptr<AudioResource>> AudioTrack::getAudioResourcesAtAbsoluteRange(juce::Range<double> rangeInSeconds) const
{
    return audioResourceContainer.getAudioResourcesForTrackAtAbsoluteRange(const_cast<AudioTrack*>(this), rangeInSeconds);
}

std::shared_ptr<AudioSubGroup> AudioTrack::getSubGroupAtAbsolutePosition(double position, audium::TimeContextType context) const
{
    std::shared_ptr<AudioSubGroup> subGroup = nullptr;
    for (auto resource : getAudioResources())
    {
        if (resource->containsAbsolutePosition(position, context))
        {
            return resource->getAudioSubGroup();
        }
    }
    return nullptr;
}

void AudioTrack::setColour(juce::Colour colour)
{
    groupColour = colour;
}

const int AudioTrack::getId() const
{
    return owner.getAudioTrackId(std::dynamic_pointer_cast<const AudioTrack>(getSharedPtr()));
}

const int AudioTrack::getChannelOffset() const
{
    return owner.getChannelOffset(std::dynamic_pointer_cast<const AudioTrack>(getSharedPtr()));
}

bool AudioTrack::writeToJson (json& output)
{
    output["name"] = name;
    output["colour"] = groupColour.toString().toStdString();
    
    for (auto channel : audioChannelContainer->getObjects())
    {
        output["channels"] += channel->data;
    }
    
    for (auto subGroup : audioSubGroupContainer->getObjects())
    {
        json j;
        subGroup->writeToJson(j);
        output["sub_groups"] += j;
    }
    
    
    playListContainer->writeToJson(output);
    
    //std::cout << output.dump(4) << std::endl;

    return true;
}

bool AudioTrack::writeChannelToJson (json& output, AudioChannel* audioChannel)
{
    
    for (auto channel : audioChannelContainer->getObjects())
    {
        if (channel.get() == audioChannel)
            output["channels"] += channel->data;
    }
    
    for (auto subGroup : audioSubGroupContainer->getObjects())
    {
        json j;
        subGroup->writeChannelToJson(j, audioChannel);
        output["sub_groups"] += j;
    }
    
    
    playListContainer->writeToJson(output);
    
    //std::cout << output.dump(4) << std::endl;

    return true;
}

bool AudioTrack::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioTrack::readFromJson (json& input, bool rebuild)
{
    //std::cout << input.dump(4) << std::endl;
    json output;
    writeToJson(output);
    if (input == output)
    {
        std::cout << "skip AudioTrack::readFromJson" << std::endl;
        return true;
    }
    
    
    if (rebuild)
        cleanup();
    
    if (input.contains("name"))
        name = input["name"].template get<std::string>();
    
    if (input.contains("colour"))
        groupColour = juce::Colour::fromString(input["colour"].template get<std::string>());
    
    // Channels
    auto jsonChannels = input["channels"];
    auto c = 0;
    for (auto& jsonElement : jsonChannels)
    {
        std::shared_ptr<AudioChannel> channel = nullptr;
        if (rebuild)
        {
            channel = addChannel();
        }
        else
        {
            channel = audioChannelContainer->getObjects()[c];
        }
        if (channel != nullptr)
            channel->data = jsonElement;
        c++;
    }
    
    // SubGroups
    auto jsonSubGroups = input["sub_groups"];
    auto i = 0;
    for (auto& jsonElement : jsonSubGroups)
    {
        std::shared_ptr<AudioSubGroup> subGroup = nullptr;
        if (rebuild)
        {
            subGroup = AudioTrackFactory::createAudioSubGroup(*this);
            audioSubGroupContainer->push_back(subGroup);
        }
        else
        {
            subGroup = audioSubGroupContainer->getObjects()[i];
        }
        
        if (subGroup != nullptr)
            if (!subGroup->readFromJson(jsonElement, rebuild))
                return false;
        
        i++;
    }
    
    // PlayList
    return playListContainer->readFromJson(input, rebuild);
}

bool AudioTrack::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        getAudioTrackContainer().sendActionMessage(rebuildAll);
        return true;
    }
    return false;
}

int AudioTrack::getSizeInUnits()
{
    return (int)audioSubGroupContainer->getObjects().size() * 16;
}

int AudioTrack::getNumAudioTrackChannels() const
{
    return static_cast<int>(audioChannelContainer->getObjects().size());
}

void AudioTrack::ensureNumChannels(int channelsNeeded)
{
    while (getNumAudioTrackChannels() < channelsNeeded)
    {
        addChannel();
    }
}

std::shared_ptr<AudioChannel> AudioTrack::addChannel()
{
    auto channel = std::shared_ptr<AudioChannel>(new AudioChannel(*this,                                                                  
                                                                  selectionManager));
    audioChannelContainer->push_back(channel);
    return channel;
}

std::shared_ptr<AudioChannel> AudioTrack::getChannel(int channelNumber) const
{
    if (channelNumber < audioChannelContainer->getObjects().size())
    {
        jassert(audioChannelContainer->getObjects()[channelNumber]->getChannelNumber() == channelNumber);
        return audioChannelContainer->getObjects()[channelNumber];
    }
    return nullptr;
}

int AudioTrack::getTotalHeight() const
{
    int height = 0;
    auto channels = getNumAudioTrackChannels();
    for (auto c = 0; c < channels; c++)
    {
        height += getChannel(c)->getChannelHeight();
    }
    return height;
}

const float AudioTrack::getOutputLevel(int channelNumber) const
{
    // adding the track's channel offset
    return transportSourceContainer->getOutputLevel(channelNumber + getChannelOffset());
}

void AudioTrack::setGain(float gain, int channelNumber) {

    if (auto channel = audioChannelContainer->objects[channelNumber]) {
        // undo
        auto action = std::make_unique<audium::UndoableContainerAction>(getAudioTrackContainer(), false);
        
        channel->setGain(gain);
        
        // undo
        action->storeNewState();
        
        getAudioTrackContainer().getUndoManager()->perform(action.release(), "Set Gain");
        getAudioTrackContainer().getUndoManager()->beginNewTransaction();
    }
    
}

float AudioTrack::getGain(int channelNumber) const {
    if (auto channel = audioChannelContainer->objects[channelNumber]) {
        return channel->getGain();
    }
    return 0.0;
}


std::shared_ptr<AudioSubGroup> AudioTrack::createNewAudioSubGroup(double transportPosition, audium::TimeContextType context)
{
    auto subGroup = AudioTrackFactory::createAudioSubGroup(*this);
    subGroup->setAbsolutePosition(transportPosition, context);
    audioSubGroupContainer->push_back(subGroup);
    return subGroup;
}

std::shared_ptr<AudioSubGroup> AudioTrack::getDefaultSubGroup() const
{
    if (audioSubGroupContainer->getObjects().size() > 0)
    {
        return audioSubGroupContainer->getObjects()[0];
    }
    jassertfalse;
    return  nullptr;
}


void AudioTrack::setSelected(bool bSelected, bool selectChildren)
{
    if (selectChildren)
    {
        audioChannelContainer->selectAllObjects(bSelected);
        audioSubGroupContainer->selectAllObjects(bSelected);
        getPlayListContainer()->playListItems.selectAllObjects(bSelected);
    }

    audium::Selectable::setSelected(bSelected, selectChildren);
}


void AudioTrack::setChannelHeight(int height)
{
    for (auto i = 0; i < getNumAudioTrackChannels(); i++)
    {
        getChannel(i)->setChannelHeight(height);
    }
}

std::list<std::shared_ptr<PositionableBase>> AudioTrack::getPositionableItems(bool arrangementMode) const
{
    // returns all positionable items
    // note: depending on the arrangement or edit mode
    
    std::list<std::shared_ptr<PositionableBase>> result;
    if (arrangementMode)
    {
        auto playListItems = getPlayListContainer()->getPlayListItems();
        for (auto playListItem : playListItems)
            result.push_back(playListItem);
    }
    else
    {
        for (auto subGroup : getAudioSubGroups())
            result.push_back(subGroup);
    }
    
    
    result.sort( []( const std::shared_ptr<PositionableBase> a, const std::shared_ptr<PositionableBase> b ) {
        return a->getAbsolutePositionRange(audium::clocks).getEnd() < b->getAbsolutePositionRange(audium::clocks).getEnd();
    } );
    
    return result;
}

bool AudioTrack::deleteSelectedObject(std::shared_ptr<audium::Selectable> object)
{
    if (AudioSubGroup* subGroup = dynamic_cast<AudioSubGroup*>(object.get()))
    {
        return audioSubGroupContainer->deleteObject(subGroup);
    }
    else if (AudioChannel* audioChannel = dynamic_cast<AudioChannel*>(object.get()))
    {
        return deleteChannel(audioChannel);
    }
    else if (AudioRegion* audioRegion = dynamic_cast<AudioRegion*>(object.get()))
    {
        return audioRegionContainer->deleteAudioRegion(audioRegion);
    }
    else if (PlayListItem* playListItem = dynamic_cast<PlayListItem*>(object.get()))
    {
        return playListContainer->deletePlayListItem(playListItem);
    }
    
    return false;
}


bool AudioTrack::deleteChannel(AudioChannel* channel) {
    
    bool result = false;

    if (audioChannelContainer->objectExists(channel)) {
        auto channelNumber = channel->getChannelNumber();
        
        audioResourceContainer.onDeleteChannel(this, channel);
        
        if (audioChannelContainer->deleteObject(channel)) {
            // change mapping
            for (auto resource : getAudioResources())
            {
                resource->getChannelMapping().decrementChannelMapping(channelNumber);
            }
            
            // cleanup subgroups
            deleteEmptySubGroups();
        }
    }
    
    return result;
}

void AudioTrack::deleteEmptySubGroups()
{
    std::vector<std::shared_ptr<AudioSubGroup>> subGroupsToDelete;
    for (auto subGroup : audioSubGroupContainer->getObjects())
    {
        if (subGroup->getAudioResources().size() == 0)
        {
            subGroupsToDelete.push_back(subGroup);
        }
    }
    
    for (auto item : subGroupsToDelete)
    {
        audioSubGroupContainer->deleteObject(item.get());
    }
}

bool AudioTrack::addAudioFiles(const juce::StringArray& filenames,
                               double position,
                               bool arrangementMode,
                               std::function<void (std::string)> callback)
{
    int channelPosition = 0;
    std::shared_ptr<AudioSubGroup> subGroup = nullptr;
    
    bool subGroupCreated = false;
    if (arrangementMode)
    {
        if (auto playListItem = getPlayListContainer()->itemAtAbsolutePosition(position,
                                                                               audium::clocks))
        {
            subGroup = playListItem->getRegion()->getAudioSubGroup();
        }
    }
    else
    {
        subGroup = getSubGroupAtAbsolutePosition(position, audium::clocks);
    }
    
    if (subGroup == nullptr)
    {
        subGroup = createNewAudioSubGroup(position, audium::clocks);
        subGroupCreated = true;
    }
    else
    {
        position = subGroup->getAbsolutePosition(audium::clocks);
        channelPosition = getNumAudioTrackChannels();
    }
    
    std::vector<std::shared_ptr<AudioResource>> resources;
    
    juce::String errors;
    
    for (auto i = 0; i < filenames.size(); i++)
    {
        if (auto resource = addAudioFile(subGroup, filenames[i], channelPosition))
        {
            resources.push_back(resource);
        }
        else
        {
            errors += filenames[i];
        }
    }
    
    if (resources.size() == 0)
    {
        NullCheckedInvocation::invoke (callback, errors.toStdString());
        return false;
    }
    
    if (arrangementMode &&
        subGroupCreated)
    {
        createDefaultPlayListItem(resources.front(), subGroup, position, audium::clocks);
    }
    
    subGroup->getAudioClip()->validateData();
    
    return (resources.size() > 0);
    
}

std::shared_ptr<AudioResource> AudioTrack::addAudioFile(std::shared_ptr<AudioSubGroup> subGroup,
                                                        const juce::File filename,
                                                        int &channelPosition)
{
    auto audioResource = getAudioResourceContainer().addAudioResource(URL (filename),
                                                                      std::dynamic_pointer_cast<AudioTrack>(getSharedPtr()),
                                                                      subGroup,
                                                                      channelPosition);
    if (audioResource != nullptr)
    {
        auto transportSource = getAudioResourceContainer().createTransportSourceForAudioResource(audioResource);
        jassert(transportSource);
        if (transportSource != nullptr)
        {
            channelPosition += audioResource->getNumChannels();
        }
    }
    return audioResource;
}

void AudioTrack::createDefaultPlayListItem(std::shared_ptr<AudioResource> audioResource,
                                           std::shared_ptr<AudioSubGroup> subGroup,
                                           double position,
                                           audium::TimeContextType context)
{
    // create default region
    juce::Range<double> defaultRange(0.0, audioResource->getFileLength(audium::seconds));
    auto region = getAudioRegionContainer()->createRegion(audioResource->getFileNameWithoutExtension(),
                                                          defaultRange,
                                                          std::dynamic_pointer_cast<AudioTrack>(getSharedPtr()),
                                                          subGroup);
    
    juce::Range<double> range(position, position + region->getRegionData(context).getLength());
    // create play list item
    getPlayListContainer()->createPlayListItemAtPositionUI(region, range, context);
}

double AudioTrack::getTotalLength(audium::TimeContextType context, bool arrangementMode) const
{
    auto result1 = 0.0;
    if (arrangementMode)
    {
        result1 = getPlayListContainer()->getTotalLength(context);
    }
    else
    {
        double totalLength = 0.0;
        for (auto subGroup : audioSubGroupContainer->getObjects())
        {
            totalLength = std::max(totalLength, subGroup->getAbsolutePositionRange(context).getEnd());
        }
        result1 = totalLength;
    }
                           
    return result1;
}


std::vector<DspClipData> AudioTrack::getDspClipVector(bool arrangementMode) const
{
    std::vector<DspClipData> result;
    DspClipData dspClipData;
    
    if (arrangementMode)
    {
        // iterate playlist
        for (const auto &item : getPlayListContainer()->getPlayListItems())
        {
            
            for (const auto &transportSource : item->getTransportSources())
            {
                dspClipData.active = true;
                auto channelPosition = transportSource->getAudioResource().getChannelMapping().getChannelPosition();
                if (audioChannelContainer->objectExistsAtIndex(channelPosition))
                    dspClipData.gain = audioChannelContainer->objects[channelPosition]->getGain();
                
                dspClipData.clipData.regionData = item->getRegionData(audium::seconds);
                dspClipData.clipData.absolutePositionClocks = item->getAbsolutePosition(audium::clocks);
                
                dspClipData.transportSourceIndex = transportSourceContainer->getTransportSourceIndex(transportSource);
                result.push_back(dspClipData);
            }
        }
    }
    else
    {
        // iterate sub groups
        for (const auto &subGroup : getAudioSubGroups())
        {
            for (const auto &transportSource : subGroup->getTransportSources())
            {
                dspClipData.clipData        = subGroup->getAudioClip()->data;
                dspClipData.active          = true;
                dspClipData.transportSourceIndex = transportSourceContainer->getTransportSourceIndex(transportSource);
                result.push_back(dspClipData);
            }
        }
    }
    
    return result;
}


