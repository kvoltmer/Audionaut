/*
  ==============================================================================

    AudioGroup.cpp
    Created: 28 Sep 2023 1:33:20pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/Factory/AudioGroupFactory.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/AudiumTransportSource.h"
#include "Engine/Undo/UndoableContainerAction.h"

AudioGroup::~AudioGroup()
{
    cleanup();
}

void AudioGroup::cleanup()
{
    audioSubGroupContainer->cleanup();
    audioChannelContainer->cleanup();
    playListContainer->playListItems.cleanup();
}


std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResources() const
{
    return audioResourceContainer.getAudioResourcesForGroup(const_cast<AudioGroup*>(this));
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtAbsoluteRange(juce::Range<double> rangeInSeconds) const
{
    return audioResourceContainer.getAudioResourcesForGroupAtAbsoluteRange(const_cast<AudioGroup*>(this), rangeInSeconds);
}

std::shared_ptr<AudioSubGroup> AudioGroup::getSubGroupAtAbsolutePosition(double position, audium::TimeContextType context) const
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

void AudioGroup::setColour(juce::Colour colour)
{
    groupColour = colour;
}

bool AudioGroup::writeToJson (json& output)
{
    output["name"] = groupName;
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

bool AudioGroup::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioGroup::readFromJson (json& input, bool rebuild)
{
    //std::cout << input.dump(4) << std::endl;
    json output;
    writeToJson(output);
    if (input == output)
    {
        std::cout << "skip AudioGroup::readFromJson" << std::endl;
        return true;
    }
    
    
    if (rebuild)
        cleanup();
    
    groupName = input["name"].template get<std::string>();
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
            subGroup = AudioGroupFactory::createAudioSubGroup(*this);
            audioSubGroupContainer->push_back(subGroup);
        }
        else
        {
            subGroup = audioSubGroupContainer->getObjects()[i];
        }
        
        if (!subGroup->readFromJson(jsonElement, rebuild))
            return false;
        
        i++;
    }
    
    // PlayList
    return playListContainer->readFromJson(input, rebuild);
}

bool AudioGroup::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        getAudioGroupContainer().sendActionMessage(rebuildAll);
        return true;
    }
    return false;
}

int AudioGroup::getSizeInUnits()
{
    return (int)audioSubGroupContainer->getObjects().size() * 16;
}

int AudioGroup::getNumChannels() const
{
    return static_cast<int>(audioChannelContainer->getObjects().size());
}

void AudioGroup::ensureNumChannels(int channelsNeeded)
{
    while (getNumChannels() < channelsNeeded)
    {
        addChannel();
    }
}

std::shared_ptr<AudioChannel> AudioGroup::addChannel()
{
    auto channel = std::shared_ptr<AudioChannel>(new AudioChannel(*this,
                                                                  (int)audioChannelContainer->getObjects().size(),
                                                                  selectionManager));
    audioChannelContainer->push_back(channel);
    return channel;
}

std::shared_ptr<AudioChannel> AudioGroup::getChannel(int channelNumber) const
{
    if (channelNumber < audioChannelContainer->getObjects().size())
    {
        jassert(audioChannelContainer->getObjects()[channelNumber]->getChannelNumber() == channelNumber);
        return audioChannelContainer->getObjects()[channelNumber];
    }
    return nullptr;
}

int AudioGroup::getTotalHeight() const
{
    int height = 0;
    auto channels = getNumChannels();
    for (auto c = 0; c < channels; c++)
    {
        height += getChannel(c)->getChannelHeight();
    }
    return height;
}

float AudioGroup::getOutputLevel(int channelNumber) const
{
    return transportSourceContainer->getOutputLevel(channelNumber);
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtChannel(int channelNumber) const
{
    auto channel = getChannel(channelNumber);
    
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto resource : getAudioResources())
    {
        if (resource->containsChannel(channel))
        {
            result.push_back(resource);
        }
    }
    return result;
}

void AudioGroup::setGain(float gain, int channelNumber)
{
    // TODO: move this to channel class
    // TODO: fixme
    auto resources = getAudioResourcesAtChannel(channelNumber);
    for (auto resource : resources)
    {
        jassertfalse;
        //resource->getAudioTransportSource()->setGain(gain);
    }
}

float AudioGroup::getGain(int channelNumber) const
{
    // TODO: move this to channel class
    // TODO: fixme
    auto resources = getAudioResourcesAtChannel(channelNumber);
//    if (resources.size() > 0 &&
//        resources[0]->getAudioTransportSource() != nullptr)
//    {
//        return resources[0]->getAudioTransportSource()->getGain();
//    }
    return 0.0;
}


std::shared_ptr<AudioSubGroup> AudioGroup::createNewAudioSubGroup(double transportPosition, audium::TimeContextType context)
{
    auto subGroup = AudioGroupFactory::createAudioSubGroup(*this);
    subGroup->getAudioClip()->setAbsolutePosition(transportPosition, context);
    audioSubGroupContainer->push_back(subGroup);
    return subGroup;
}

std::shared_ptr<AudioSubGroup> AudioGroup::getDefaultSubGroup() const
{
    if (audioSubGroupContainer->getObjects().size() > 0)
    {
        return audioSubGroupContainer->getObjects()[0];
    }
    jassertfalse;
    return  nullptr;
}


void AudioGroup::setSelected(bool bSelected, bool selectChildren)
{
    if (selectChildren)
    {
        audioChannelContainer->selectAllObjects(bSelected);
        audioSubGroupContainer->selectAllObjects(bSelected);
        getPlayListContainer()->playListItems.selectAllObjects(bSelected);
    }

    audium::Selectable::setSelected(bSelected, selectChildren);
}


void AudioGroup::setChannelHeight(int height)
{
    for (auto i = 0; i < getNumChannels(); i++)
    {
        getChannel(i)->setChannelHeight(height);
    }
}

std::list<std::shared_ptr<PositionableBase>> AudioGroup::getPositionableItems(bool arrangementMode) const
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
            result.push_back(subGroup->getAudioClip());
    }
    
    
    result.sort( []( const std::shared_ptr<PositionableBase> a, const std::shared_ptr<PositionableBase> b ) {
        return a->getAbsolutePositionRange(audium::clocks).getEnd() < b->getAbsolutePositionRange(audium::clocks).getEnd();
    } );
    
    return result;
}

bool AudioGroup::deleteSelectedObject(std::shared_ptr<audium::Selectable> object)
{
    // TODO: proove if object exits. Unless the dynamic_cast may crash
    
    
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


bool AudioGroup::deleteChannel(AudioChannel* channel) {
    
    bool result = false;
    if (audioChannelContainer->objectExists(channel))
    {
        audioResourceContainer.onDeleteChannel(channel);
        result = audioChannelContainer->deleteObject(channel);
    }
    
    auto count = 0;
    for (auto channel : audioChannelContainer->getObjects())
    {
        channel->setChannelNumber(count++);
    }

    // cleanup subgroups
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
    
    return result;
}

bool AudioGroup::addAudioFiles(const juce::StringArray& filenames,
                               double position,
                               bool arrangementMode,
                               std::function<void (std::string)> callback)
{
    int channelPosition = 0;
    std::shared_ptr<AudioSubGroup> subGroup = nullptr;
    
    if (arrangementMode)
    {
        // try to figure out the position of the subGroup (edit mode!)
        auto items = getPositionableItems(false);
        auto subGroupPosition = 0.0;
        if (items.size() > 0)
        {
            subGroupPosition = items.back()->getAbsolutePositionRange(audium::clocks).getEnd();
        }
        subGroup = createNewAudioSubGroup(subGroupPosition, audium::clocks);
    }
    else
    {
        subGroup = getSubGroupAtAbsolutePosition(position, audium::clocks);
        if (subGroup == nullptr)
        {
            subGroup = createNewAudioSubGroup(position, audium::clocks);
        }
        else
        {
            position = subGroup->getAudioClip()->getAbsolutePosition(audium::clocks);
            channelPosition = getNumChannels();
        }
    }

    
    
    std::vector<std::shared_ptr<AudioResource>> resources;
    
    for (auto i = 0; i < filenames.size(); i++)
    {
        if (auto resource = addAudioFile(subGroup, filenames[i], channelPosition))
        {
            resources.push_back(resource);
        }
        else
        {
            NullCheckedInvocation::invoke (callback, filenames[i].toStdString());
            return false;
        }
    }
    
    if (arrangementMode &&
        resources.size() > 0)
    {
        createDefaultPlayListItem(resources.front(), subGroup, position, audium::clocks);
    }
    
    subGroup->getAudioClip()->validateData();
    
    return (resources.size() > 0);
    
}

std::shared_ptr<AudioResource> AudioGroup::addAudioFile(std::shared_ptr<AudioSubGroup> subGroup,
                                                        const juce::File filename,
                                                        int &channelPosition)
{
    auto audioResource = getAudioResourceContainer().addAudioResource(URL (filename),
                                                                      std::dynamic_pointer_cast<AudioGroup>(getSharedPtr()),
                                                                      subGroup,
                                                                      channelPosition);
    if (audioResource != nullptr)
    {
        getAudioResourceContainer().createTransportSourceForAudioResource(audioResource);
        channelPosition += audioResource->getNumChannels();
    }
    return audioResource;
}

void AudioGroup::createDefaultPlayListItem(std::shared_ptr<AudioResource> audioResource,
                                           std::shared_ptr<AudioSubGroup> subGroup,
                                           double position,
                                           audium::TimeContextType context)
{
    // create default region
    juce::Range<double> defaultRange(0.0, audioResource->getFileLength(audium::seconds));
    auto region = getAudioRegionContainer()->createRegion(audioResource->getFileNameWithoutExtension(),
                                                          defaultRange,
                                                          std::dynamic_pointer_cast<AudioGroup>(getSharedPtr()),
                                                          subGroup);
    
    juce::Range<double> range(position, position + region->getRegionData(context).getLength());
    // create play list item
    getPlayListContainer()->createPlayListItemAtPositionUI(region, range, context);
}

double AudioGroup::getTotalLength(audium::TimeContextType context, bool arrangementMode) const
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
            totalLength = std::max(totalLength, subGroup->getAudioClip()->getAbsolutePositionRange(context).getEnd());
        }
        result1 = totalLength;
    }
                           
    return result1;
}


std::vector<DspClipData> AudioGroup::getDspClipVector(bool arrangementMode) const
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
                dspClipData.clipData.regionData = item->getRegionData(audium::seconds);
                dspClipData.clipData.absolutePositionClocks = item->getAbsolutePosition(audium::clocks);
                dspClipData.active          = true;
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


