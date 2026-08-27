//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Analysis/AnalysisProvider.h"

namespace audium {


AudioTrack::~AudioTrack()
{
    cleanup();
}

void AudioTrack::cleanup()
{
    resourceGroupContainer->cleanup();
    audioChannelContainer->cleanup();
    playListContainer->playListItems.cleanup();
    getAudioResourceContainer().removeAudioResourcesForTrack(this);
}

void AudioTrack::setAudioTrackName(const juce::String newName)
{
    name = newName.toStdString();
}

std::vector<std::shared_ptr<AudioResource>> AudioTrack::getAudioResources() const
{
    return audioResourceContainer.getAudioResourcesForTrack(const_cast<AudioTrack*>(this));
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
    viewState.writeToJson(output);

    for (auto channel : audioChannelContainer->getObjects())
    {
        output["channels"] += channel->data;
    }
    
    for (auto resourceGroup : resourceGroupContainer->getObjects())
    {
        json j;
        resourceGroup->writeToJson(j);
        output["resource_groups"] += j;
    }
    
    
    playListContainer->writeToJson(output);
    
    //std::cout << output.dump(4) << std::endl;
    
    return true;
}

bool AudioTrack::writeChannelToJson (json& output, AudioChannel* audioChannel)
{
    for (auto channel : audioChannelContainer->getObjects()) {
        if (channel.get() == audioChannel)
            output["channels"] += channel->data;
    }
    
    for (auto resourceGroup : resourceGroupContainer->getObjects()) {
        json j;
        resourceGroup->writeChannelToJson(j, audioChannel);
        output["resource_groups"] += j;
    }

    playListContainer->writeChannelToJson(output, audioChannel);

    //std::cout << output.dump(4) << std::endl;

    return true;
}

void AudioTrack::mergeChannelFromJson(json& input)
{
    // merge channels
    auto jsonChannels = input["channels"];
    
    auto destChannel = -1;
    for (auto& jsonElement : jsonChannels) {
        auto channel = addChannel();
        destChannel = channel->getChannelNumber();
        channel->data = jsonElement;
        channel->commitChannelData();
    }
    jassert(destChannel != -1);
    
    // merge resource groups
    json jsonResourceGroups;
    if (input.contains("resource_groups")) {
        jsonResourceGroups = input["resource_groups"];
    }
    else if (input.contains("sub_groups")) {
        jsonResourceGroups = input["sub_groups"]; // legacy support
    }
    else {
        jassertfalse;
    }
    
    for (auto& jsonResourceGroup : jsonResourceGroups) {
        auto resourceGroup = findExistingResourceGroup(jsonResourceGroup);
        if (resourceGroup == nullptr) {
            resourceGroup = createNewResourceGroup();
        }
        resourceGroup->mergeFromJson(jsonResourceGroup, destChannel);
    }
    // merge playlist
    getPlayListContainer()->mergeFromJson(input, destChannel);
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
        //std::cout << "skip AudioTrack::readFromJson" << std::endl;
        return true;
    }
    
    
    if (rebuild)
        cleanup();
    
    if (input.contains("name"))
        name = input["name"].template get<std::string>();

    viewState.readFromJson(input);

    // Channels
    auto jsonChannels = input["channels"];
    
    if (!rebuild && jsonChannels.size() != audioChannelContainer->size()) {
        rebuild = true;
        audioChannelContainer->cleanup();
    }
    auto c = 0;
    for (auto& jsonElement : jsonChannels) {
        std::shared_ptr<AudioChannel> channel = nullptr;
        if (rebuild) {
            channel = addChannel();
        }
        else {
            channel = audioChannelContainer->getObjects()[c];
        }
        if (channel != nullptr) {
            channel->data = jsonElement;
            channel->commitChannelData();
        }
        c++;
    }
    
    // ResourceGroups
    json jsonResourceGroups;
    if (input.contains("resource_groups")) {
        jsonResourceGroups = input["resource_groups"];
    }
    else if (input.contains("sub_groups")) {
        jsonResourceGroups = input["sub_groups"]; // legacy support
    }
    
    if (!rebuild && jsonResourceGroups.size() != resourceGroupContainer->size()) {
        rebuild = true;
        resourceGroupContainer->cleanup();
    }
    auto i = 0;
    for (auto& jsonElement : jsonResourceGroups)
    {
        std::shared_ptr<ResourceGroup> resourceGroup = nullptr;
        if (rebuild)
        {
            resourceGroup = AudioTrackFactory::createResourceGroup(*this);
            resourceGroupContainer->push_back(resourceGroup);
        }
        else
        {
            resourceGroup = resourceGroupContainer->getObjects()[i];
        }
        
        if (resourceGroup != nullptr)
            if (!resourceGroup->readFromJson(jsonElement, rebuild))
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

int AudioTrack::getNumAudioTrackChannels() const
{
    return static_cast<int>(audioChannelContainer->getObjects().size());
}

void AudioTrack::ensureNumChannels(int channelsNeeded)
{
    while (getNumAudioTrackChannels() < channelsNeeded)
    {
        if (addChannel() == nullptr)
            break;
    }
}

std::shared_ptr<AudioChannel> AudioTrack::addChannel()
{
    if (getAudioTrackContainer().getNumAudioTrackChannels() < MAX_AUDIO_CHANNELS) {
        auto channel = std::make_shared<AudioChannel>(*this,
                                                      selectionManager,
                                                      getAudioTrackContainer().audioBusInterface);
        audioChannelContainer->push_back(channel);
        return channel;
    }
    std::cout << "error: max audio channels reached: " << getAudioTrackContainer().getNumAudioTrackChannels() << std::endl;
    return nullptr;
}

std::shared_ptr<AudioChannel> AudioTrack::getChannel(int channelNumber) const
{
    if ((std::size_t)channelNumber < audioChannelContainer->getObjects().size())
    {
        jassert(audioChannelContainer->getObject((std::size_t)channelNumber)->getChannelNumber() == channelNumber);
        return audioChannelContainer->getObject((std::size_t)channelNumber);
    }
    return nullptr;
}

void AudioTrack::setGain(float gain, int channelNumber) {
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        channel->setGain(gain);
    }
}

float AudioTrack::getGain(int channelNumber) const {
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        return channel->getGain();
    }
    return 0.0;
}

void AudioTrack::setPan(float pan, int channelNumber) {
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        channel->setPan(pan);
    }
}

float AudioTrack::getPan(int channelNumber) const {
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        return channel->getPan();
    }
    return 0.0;
}

void AudioTrack::setMute(bool bMute, int channelNumber) {
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        channel->setMute(bMute);
    }
}

bool AudioTrack::getMute(int channelNumber) const {
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        return channel->getMute();
    }
    return 0.0;
}

void AudioTrack::setSolo(bool bSolo, int channelNumber) {
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        channel->setSolo(bSolo);
    }
}

bool AudioTrack::getSolo(int channelNumber) const {
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        return channel->getSolo();
    }
    return 0.0;
}

void AudioTrack::setChannelData(const int channelNumber, const AudioChannelData data)
{
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        channel->data = data;
        channel->commitChannelData();
    }
}

const AudioChannelData AudioTrack::getChannelData(const int channelNumber) const
{
    if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
        return channel->data;
    }
    return AudioChannelData();
}

void AudioTrack::setRecordEnabled(const int channelNumber, bool bEnabled)
{
    if (channelNumber >= 0) {
        if (auto channel = audioChannelContainer->getObject((std::size_t)channelNumber)) {
            channel->setRecordEnabled(bEnabled);
        }
    }
    else {
        // all channels
        for (auto channel : audioChannelContainer->objects) {
            channel->setRecordEnabled(bEnabled);
        }
    }
}

bool AudioTrack::isRecordEnabled(const int channelNumber)
{
    if (channelNumber > 0 && channelNumber < (int)audioChannelContainer->objects.size()) {
        return audioChannelContainer->getObject((std::size_t)channelNumber)->isRecordEnabled();
    }
    else {
        for (auto chan : audioChannelContainer->objects) {
            if (chan->isRecordEnabled())
                return true;
        }
        
        return false;
    }
}

bool AudioTrack::isRecording(const int channelNumber) const
{
    if (channelNumber > 0 && (std::size_t)channelNumber < audioChannelContainer->objects.size()) {
        return audioChannelContainer->getObject((std::size_t)channelNumber)->isRecording();
    }
    else {
        for (auto chan : audioChannelContainer->objects) {
            if (chan->isRecording())
                return true;
        }
        
        return false;
    }
}

void AudioTrack::onDragStart()
{
    undoableContainerAction = std::make_unique<audium::UndoableContainerAction>(getAudioTrackContainer(), false);
}

void AudioTrack::onDragEnd()
{
    if (undoableContainerAction != nullptr) {
        undoableContainerAction->storeNewState();
        getAudioTrackContainer().getUndoManager()->perform(undoableContainerAction.release(),
                                                           "Set Track Parameter");
        getAudioTrackContainer().getUndoManager()->beginNewTransaction();
    }
}

std::shared_ptr<ResourceGroup> AudioTrack::createNewResourceGroup()
{
    auto resourceGroup = AudioTrackFactory::createResourceGroup(*this);
    resourceGroupContainer->push_back(resourceGroup);
    return resourceGroup;
}

std::shared_ptr<ResourceGroup> AudioTrack::createNewResourceGroup(const std::shared_ptr<ResourceGroup> otherResourceGroup)
{
    json otherGroupJson;
    otherResourceGroup->writeToJson(otherGroupJson);
    auto resourceGroup = findExistingResourceGroup(otherGroupJson);
    
    if (resourceGroup == nullptr) {
        resourceGroup = AudioTrackFactory::createResourceGroup(*this);
        
        // clone all resources
        auto otherResources = otherResourceGroup->getAudioResources();
        for (auto resource : otherResources) {
            
            auto newResource = resourceGroup->addAudioResourceFromUrl(resource->getUrl());
            auto srcChannel = resource->getChannelMapping().getSourceChannel();
            auto dstChannel = resource->getChannelMapping().getDestinationChannel();
            newResource->getChannelMapping().setOutputChannelMapping(srcChannel, dstChannel);
        }
        
        resourceGroupContainer->push_back(resourceGroup);
    }
    
    return resourceGroup;
}

std::shared_ptr<ResourceGroup> AudioTrack::findExistingResourceGroup(json jsonResourceGroup)
{
    // iterate over resource groups and compare it's regions by name, start and end
    for (auto resourceGroup : resourceGroupContainer->getObjects()) {
        auto regionContainer = resourceGroup->getAudioRegionContainer();
        auto otherRegions = jsonResourceGroup["regions"];
        auto numRegions = static_cast<size_t>(regionContainer->getNumRegions());
        if (numRegions == otherRegions.size()) {
            
            bool success = true;
            // check name, start, end
            for (size_t i = 0; i < numRegions; ++i) {
                auto r = regionContainer->getRegion((int)i);
                if (r->getName().toStdString() != otherRegions[i].at("name").get<std::string>()) {
                    success = false;
                    break;
                }
                
                if (abs(otherRegions[i].at("start").get<double>() - r->getRegionData(audium::seconds).getStart()) > 0.0) {
                    success = false;
                    break;
                }

                if (abs(otherRegions[i].at("end").get<double>() - r->getRegionData(audium::seconds).getEnd()) > 0.0) {
                    success = false;
                    break;
                }
            }
            
            if (success) {
                return resourceGroup;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<ResourceGroup> AudioTrack::getDefaultResourceGroup() const
{
    if (resourceGroupContainer->getObjects().size() > 0)
    {
        return resourceGroupContainer->getObjects()[0];
    }
    jassertfalse;
    return  nullptr;
}


void AudioTrack::setSelected(bool bSelected)
{    
    audium::Selectable::setSelected(bSelected);
}


std::list<std::shared_ptr<PositionableBase>> AudioTrack::getPositionableItems() const
{
    // returns all positionable items
    
    std::list<std::shared_ptr<PositionableBase>> result;
    
    auto playListItems = getPlayListContainer()->getPlayListItems();
    for (auto playListItem : playListItems)
        result.push_back(playListItem);
    
    result.sort( []( const std::shared_ptr<PositionableBase> a, const std::shared_ptr<PositionableBase> b ) {
        return a->getAbsolutePositionRange(audium::clocks).getEnd() < b->getAbsolutePositionRange(audium::clocks).getEnd();
    } );
    
    return result;
}

bool AudioTrack::deleteSelectedObject(std::shared_ptr<audium::Selectable> object, bool &rebuild)
{
    if (ResourceGroup* resourceGroup = dynamic_cast<ResourceGroup*>(object.get()))
    {
        rebuild = true;
        return resourceGroupContainer->deleteObject(resourceGroup);
    }
    else if (AudioChannel* audioChannel = dynamic_cast<AudioChannel*>(object.get()))
    {
        rebuild = true;
        return deleteChannel(audioChannel);
    }
    else if (AudioRegion* audioRegion = dynamic_cast<AudioRegion*>(object.get()))
    {
        rebuild = false;
        auto result = audioRegion->getResourceGroup()->getAudioRegionContainer()->deleteAudioRegion(audioRegion);
        deleteUnusedResourceGroups();
        return result;
    }
    else if (PlayListItem* playListItem = dynamic_cast<PlayListItem*>(object.get()))
    {
        rebuild = false;
        return playListContainer->deletePlayListItem(playListItem, false);
    }
    
    return false;
}


bool AudioTrack::deleteChannel(AudioChannel* channel) {
    
    bool result = false;
    
    if (audioChannelContainer->objectExists(channel)) {
        const auto channelNumber = channel->getChannelNumber();
        audioResourceContainer.onDeleteChannel(this, channel);
        
        if (audioChannelContainer->deleteObject(channel)) {
            // mapping changes for resources
            for (auto resource : getAudioResources()) {
                resource->getChannelMapping().decrementDestinationChannel(channelNumber);
            }
            
            // gain vector changes for playlist items
            for (const auto &item : getPlayListContainer()->getPlayListItems()) {
                item->getDynamics().onDeleteChannel(channelNumber);
            }
            
            // cleanup subgroups
            deleteEmptyResourceGroups();
        }
    }
    
    return result;
}

void AudioTrack::deleteEmptyResourceGroups()
{
    std::vector<std::shared_ptr<ResourceGroup>> resourceGroupsToDelete;
    for (auto resourceGroup : resourceGroupContainer->getObjects()) {
        if (resourceGroup->getAudioResources().size() == 0) {
            resourceGroupsToDelete.push_back(resourceGroup);
        }
    }
    
    for (auto item : resourceGroupsToDelete) {
        resourceGroupContainer->deleteObject(item.get());
    }
}

void AudioTrack::deleteUnusedResourceGroups()
{
    std::vector<std::shared_ptr<ResourceGroup>> resourceGroupsToDelete;
    for (auto resourceGroup : resourceGroupContainer->getObjects()) {
        if (resourceGroup->getAudioRegionContainer()->getNumRegions() == 0) {
            resourceGroupsToDelete.push_back(resourceGroup);
        }
    }
    
    for (auto item : resourceGroupsToDelete) {
        resourceGroupContainer->deleteObject(item.get());
    }
}

bool AudioTrack::addAudioFiles(const juce::StringArray& filenames,
                               double position,
                               std::function<void (std::string)> callback,
                               bool undo)
{
    // Undo: store old state
    auto action = undo ? std::make_unique<audium::UndoableContainerAction>(getAudioTrackContainer()) : nullptr;
    
    int destChannel = 0;
    std::shared_ptr<ResourceGroup> resourceGroup = nullptr;
    
    bool subGroupCreated = false;

    if (auto playListItem = getPlayListContainer()->itemAtAbsolutePosition(position,
                                                                           audium::clocks)) {
        resourceGroup = playListItem->getRegion()->getResourceGroup();
    }
 
    if (resourceGroup == nullptr) {
        resourceGroup = createNewResourceGroup();
        subGroupCreated = true;
    }
    else {
        destChannel = getNumAudioTrackChannels();
    }
    
    std::vector<std::shared_ptr<AudioResource>> resources;
    
    juce::String errors;
    
    for (auto i = 0; i < filenames.size(); i++) {
        auto audioFileResources = addAudioFile(resourceGroup, filenames[i], destChannel);
        if (audioFileResources.size() > 0) {
            for (auto resource : audioFileResources)
                resources.push_back(resource);
        }
        else {
            errors += filenames[i];
        }
    }
    
    if (resources.size() == 0)
    {
        NullCheckedInvocation::invoke (callback, errors.toStdString());
        return false;
    }
    
    if (subGroupCreated)
    {
        createDefaultPlayListItem(resources.front(), resourceGroup, position, audium::clocks);
    }
        
    if (action != nullptr) {
        action->storeNewState();
        getAudioTrackContainer().getUndoManager()->perform(action.release(), "File(s) added");
        getAudioTrackContainer().getUndoManager()->beginNewTransaction();
    }
    return resources.size() > 0;
}

std::vector<std::shared_ptr<AudioResource>> AudioTrack::addAudioFile (std::shared_ptr<ResourceGroup> resourceGroup,
                                                                      const juce::File filename,
                                                                      int &destChannel)
{
    std::vector<std::shared_ptr<AudioResource>> result;
    auto numResourcesLoaded = getAudioResources().size();
    auto url = URL (filename);
    auto chan = destChannel;
    getAudioResourceContainer().copyToAudioFileDirectoryIfNeeded(url);
    if (auto audioFormatReader = getAudioResourceContainer().getAudioFormatReaderForUrl(url)) {
        for (auto sourceChannel = 0; sourceChannel < audioFormatReader->numChannels; sourceChannel++) {
            auto audioResource = getAudioResourceContainer().addAudioResource(url,
                                                                              audioFormatReader,
                                                                              std::dynamic_pointer_cast<AudioTrack>(getSharedPtr()),
                                                                              resourceGroup,
                                                                              destChannel,
                                                                              sourceChannel);
            if (audioResource != nullptr) {
                auto voiceSource = getAudioResourceContainer().createTransportSourceForAudioResource(audioResource);
                if (voiceSource != nullptr)
                    destChannel += 1;
                
                result.push_back(audioResource);
            }
        }
        
        
        // apply stereo pan
        if (numResourcesLoaded == 0 &&
            audioFormatReader->numChannels == 2) {
            setPan(-1.f, chan);
            setPan(1.f, chan + 1);
        }
    }
    
    return result;
}

std::shared_ptr<PlayListItem> AudioTrack::createDefaultPlayListItem(std::shared_ptr<AudioResource> audioResource,
                                           std::shared_ptr<ResourceGroup> resourceGroup,
                                           double position,
                                           audium::TimeContextType context)
{
    // create default region
    juce::Range<double> defaultRange(0.0, audioResource->getFileLength(context));
    auto region = resourceGroup->getAudioRegionContainer()->createRegion(  audioResource->getFileNameWithoutExtension(),
                                                                    defaultRange,
                                                                    std::dynamic_pointer_cast<AudioTrack>(getSharedPtr()),
                                                                    resourceGroup,
                                                                    nullptr,
                                                                    context);
    
    // an empty name indicates a recording
    if (region->getName().isEmpty()) {
        jassert(audioResource->isRecording());
        auto take = AudiumEngine::recordingCounter;
        auto takeString = juce::String(AudioRegionContainer::formatNumber(take + 1));
        
        // Format as [Year-Month-Day Hour-Minute-Second]
        auto now = juce::Time::getCurrentTime();
        auto formattedStr = now.formatted ("[%Y-%m-%d_%H-%M-%S]");
        region->setName("take " + takeString + " " + formattedStr);
    }
    
    
    // create play list item
    auto newItem = getPlayListContainer()->createPlayListItemAtPositionUI(region, position, context);
    getPlayListContainer()->sortByPosition();
    return newItem;
}

double AudioTrack::getTotalLength(audium::TimeContextType context) const
{
    return getPlayListContainer()->getTotalLength(context);
}


std::vector<DspClipData> AudioTrack::getDspClipVector() const
{
    std::vector<DspClipData> result;
    DspClipData dspClipData;
    
    // iterate playlist items and it's transport sources
    for (const auto &item : getPlayListContainer()->getPlayListItems()) {
        for (const auto &voiceSource : item->getVoiceSources()) {
            if (voiceSource != nullptr) {
                dspClipData.active              = true;
                auto channel = voiceSource->getAudioResource().getChannelMapping().getDestinationChannel();
                dspClipData.clipGain            = static_cast<float>(item->getDynamics().getGain(channel));
                dspClipData.clipFadeInClocks    = item->getDynamics().getFadeIn(audium::clocks);
                dspClipData.clipFadeOutClocks   = item->getDynamics().getFadeOut(audium::clocks);
                dspClipData.clipFadeInStartClocks = item->getDynamics().getFadeInStart(audium::clocks);
                dspClipData.clipFadeOutEndClocks  = item->getDynamics().getFadeOutEnd(audium::clocks);
                dspClipData.clipFadeInCurve       = item->getDynamics().getFadeInCurve();
                dspClipData.clipFadeOutCurve      = item->getDynamics().getFadeOutCurve();
                dspClipData.clipData.regionData = item->getRegionData(audium::seconds);
                dspClipData.clipData.absolutePositionClocks = item->getAbsolutePosition(audium::clocks);
                
                dspClipData.voiceSourceIndex = voiceSourceContainer->getVoiceSourceIndex(voiceSource);
                result.push_back(dspClipData);
            }
        }
    }

    return result;
}

void AudioTrack::dropSelectedAudioRegions(int insertIndex)
{
    auto selectedObjects = getSelectionManager()->getSelectedObjects();
    
    for (auto object : selectedObjects) {
        if (auto region = std::dynamic_pointer_cast<AudioRegion>(object)) {
            
            auto newRegion = region;
            if (this != region->getAudioTrack().get()) {
                // create new sub group
                auto resourceGroup = createNewResourceGroup(region->getResourceGroup());
                newRegion = resourceGroup->getAudioRegionContainer()->createRegion(std::dynamic_pointer_cast<AudioTrack>(getSharedPtr()),
                                                                              resourceGroup,
                                                                              region);
            }
            
            playListContainer->createPlayListItemUI(newRegion, insertIndex);
            insertIndex++;
        }
    }
}

void AudioTrack::dropSelectedAudioRegions(double pos, audium::TimeContextType context)
{
    // drop all selected regions
    auto selectedObjects = getSelectionManager()->getSelectedObjects();
    jassert(selectedObjects.size() > 0);
    
    std::vector<std::shared_ptr<AudioRegion>> selectedRegions;
    for (auto object : selectedObjects) {
        if (auto region = std::dynamic_pointer_cast<AudioRegion>(object)) {
            selectedRegions.push_back(region);
        }
    }
    // sort selected regions by id
    std::sort(selectedRegions.begin(), selectedRegions.end(),
              [this](const std::shared_ptr<AudioRegion> i1, const std::shared_ptr<AudioRegion> i2)
              {
        return (getAudioRegionId(i1) < getAudioRegionId(i2));
    });
    
    for (auto region : selectedRegions) {
        if (this != region->getAudioTrack().get()) {
            // create new sub group
            auto resourceGroup = createNewResourceGroup(region->getResourceGroup());
            region = resourceGroup->getAudioRegionContainer()->createRegion(std::dynamic_pointer_cast<AudioTrack>(getSharedPtr()),
                                                                       resourceGroup,
                                                                       region);
        }
        
        getPlayListContainer()->createPlayListItemAtPositionUI(region, pos, context);
        pos += region->getRegionData(context).getLength();
    }
    
    getPlayListContainer()->sortByPosition();
}

void AudioTrack::dropPlayListItem(std::shared_ptr<PlayListItem> item,
                                  double pos,
                                  audium::TimeContextType context,
                                  bool newPlayListItem)
{
    auto region = item->getRegion();
    
    if (newPlayListItem) {
        if (auto newItem = getPlayListContainer()->createPlayListItemAtPositionUI(region, pos, context)) {
            newItem->getDynamics().copyGainsFrom(item->getDynamics());
        }
    }
    else if (this != region->getAudioTrack().get()) { // drag on different track -> create region and play list item
        // create new sub group
        auto resourceGroup = createNewResourceGroup(region->getResourceGroup());
        if (auto newRegion = resourceGroup->getAudioRegionContainer()->createRegion(std::dynamic_pointer_cast<AudioTrack>(getSharedPtr()),
                                                                               resourceGroup,
                                                                               region)) {
            auto newItem = getPlayListContainer()->createPlayListItemAtPositionUI(newRegion, pos, context);
            newItem->getDynamics().copyGainsFrom(item->getDynamics());
            item->setSelected(false);
            newItem->setSelected(true);
            if (!ModifierKeys::currentModifiers.isAltDown()) {
                region->getAudioTrack()->getPlayListContainer()->deletePlayListItem(item.get(), false);
            }
        }
    }
    else {
        // same track
        if (ModifierKeys::currentModifiers.isAltDown()) {
            // copy
            if (auto newItem = getPlayListContainer()->createPlayListItemAtPositionUI(region, pos, context)) {
                newItem->getDynamics().copyGainsFrom(item->getDynamics());
            }
        }
        else {
            // move
            item->setAbsolutePosition(pos, context);
        }
    }
    getPlayListContainer()->sortByPosition();
}

std::shared_ptr<AudioRegion> AudioTrack::getRegion(int rowNumber) const
{
    auto regions = getRegions();
    if (rowNumber >= 0 && rowNumber < regions.size()) {
        return regions[rowNumber];
    }
    return nullptr;
}

const std::vector<std::shared_ptr<AudioRegion>> AudioTrack::getRegions() const
{
    std::vector<std::shared_ptr<AudioRegion>> result;
    
    for (auto resourceGroup : resourceGroupContainer->getObjects()) {
        for (auto region : resourceGroup->getAudioRegionContainer()->getObjects()) {
            result.push_back(region);
        }
    }
    return result;
}

int AudioTrack::getAudioRegionId(std::shared_ptr<const AudioRegion> searchRegion) const
{
    auto regions = getRegions();
    auto it = std::find(regions.begin(), regions.end(), searchRegion);
    if (it != regions.end())
        return static_cast<int>(std::distance(regions.begin(), it));
    
    return -1; // not found
}

} // namespace audium

