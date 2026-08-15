//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Group/AudioRegionAdapter.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"

namespace audium {

PlayListContainer::~PlayListContainer()
{
    playListItems.cleanup();
}

std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItemAtPositionUI(std::shared_ptr<AudioRegion> audioRegion,
                                                                                double position,
                                                                                audium::TimeContextType context)
{
    // find the insert index based on position
    auto insertIndex = 0;
    // want all items before the end of this position
    auto items = itemsAtAbsoluteRange(juce::Range<double>(0.0,
                                                          position + audioRegion->getRegionData(context).getLength()), context);
    if (items.size() > 0) {
        insertIndex = getPlayListItemIndex(items.back()) + 1;
    }
    //std::cout << "insert index: " << insertIndex << std::endl;
    
    if (auto playListItem = createPlayListItem(audioRegion, insertIndex)) {
        playListItem->setAbsolutePosition(position, context);
        // createPlayListItem() sorted while the item still sat at its
        // provisional position; only now does it hold the position it was
        // asked for, so the order has to be restored here.
        sortByPosition();
        return playListItem;
    }

    return nullptr;
}

std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItemUI(std::shared_ptr<AudioRegion> region, int insertIndex)
{
    jassert(region);
    auto playListItem = createPlayListItem(region, insertIndex);
    movePlayListItemsPosition(getPlayListItemIndex(playListItem.get()));
    return playListItem;
}

std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItem(std::shared_ptr<AudioRegion> audioRegion,
                                                                    int insertIndex)
{
    jassert( insertIndex >= 0);
    jassert( insertIndex <= playListItems.size());
    
    auto itemBefore = getPlayListItem(insertIndex);
    auto thisIsTheEnd = (playListItems.size() > 0) ? playListItems.objects.back()->getAbsolutePositionRange(audium::clocks).getEnd() : 0.0;
    
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*this,
                                                                       audioRegion,
                                                                       audioRegion->getAudioTrack()->getSelectionManager()));
    playListItems.objects.insert(playListItems.objects.begin() + insertIndex, playListItem);
    
    auto pos = 0.0;
    
    if (itemBefore != nullptr)
        pos = itemBefore->getAbsolutePositionRange(audium::clocks).getStart();
    else
        pos = thisIsTheEnd;
    
    
    // in case we insert at begin
    if (insertIndex == 0) {
        auto length = playListItem->getAbsolutePositionRange(audium::clocks).getLength();
        if (pos - length >= 0.0)
            pos -= length;
    }
    
    playListItem->setAbsolutePosition(pos, audium::clocks);
    
    sortByPosition();
    
    return playListItem;
}

std::shared_ptr<PlayListItem> PlayListContainer::clonePlayListItem(std::shared_ptr<PlayListItem> item)
{
    auto context = audium::clocks;
    
    auto resourceGroup = item->getRegion()->getResourceGroup();
    auto regionContainer = resourceGroup->getAudioRegionContainer();
    auto name = regionContainer->getUniqueName(item->getRegion()->getName());
    
    auto track = item->getRegion()->getAudioTrack();
    auto newRegion = regionContainer->createRegion(name,
                                                   item->getRegionData(context),
                                                   track,
                                                   resourceGroup,
                                                   item->getRegion(),
                                                   context);

    return createPlayListItemAtPositionUI(newRegion, item->getAbsolutePosition(context), context);
}

void PlayListContainer::movePlayListItemsPosition(int startIndex)
{
    // try to make way for item at startIndex
    auto context = audium::clocks;
    auto item = playListItems.objects.begin() + startIndex;
    if (item != playListItems.objects.end()) {
        auto range = (*item)->getAbsolutePositionRange(context);
        auto length = range.getLength();
        
        for (auto iter = playListItems.objects.begin() + startIndex + 1; iter != playListItems.objects.end(); iter++) {
            if ((*iter)->getAbsolutePositionRange(context).intersects(range)) {
                (*iter)->moveAbsolutePosition(length, context);
                std::cout << "moveAbsolutePosition: " << getPlayListItemIndex((*iter).get()) << " " << length << std::endl;
                range = (*iter)->getAbsolutePositionRange(context);
            }
            else {
                break;
            }
        }
    }
    jassert(sortedByPosition());
}

void PlayListContainer::movePlayListItemBefore(int currentIndex, int indexOfItemToPlaceBefore)
{
    auto context = audium::clocks;
    
    jassert(getPlayListItem(currentIndex));
    
    auto currentItem = getPlayListItem(currentIndex);
    auto beforeItem = getPlayListItem(indexOfItemToPlaceBefore);

    auto newPos = 0.0;
    if (beforeItem != nullptr)
        newPos = beforeItem->getAbsolutePosition(context);
    else
        newPos = playListItems.objects.back()->getAbsolutePositionRange(context).getEnd();
    
    audium::MoveItemBefore(playListItems.objects,
                           currentIndex,
                           indexOfItemToPlaceBefore);
    

    // set the new position of the dragged (current) item:
    currentItem->setAbsolutePosition(newPos, context);
    

    // move all other items to the right by duration of current item
    if (beforeItem != nullptr) {
        auto startIndex = getPlayListItemIndex(beforeItem.get());
        for (auto iter = playListItems.objects.begin() + startIndex; iter != playListItems.objects.end(); iter++) {
            (*iter)->moveAbsolutePosition(currentItem->getDurationTime(context), context);
        }
    }
    
    sortByPosition();
    
}

bool PlayListContainer::deletePlayListItem(PlayListItem* playListItem, bool deleteRegion) {
    
    if (deleteRegion) {
        for (auto resourceGroup : audioTrack.getResourceGroups()) {
            resourceGroup->getAudioRegionContainer()->deleteAudioRegion(playListItem->getRegion());
        }
    }
    
    return playListItems.deleteObject(playListItem);
}

void PlayListContainer::deletePlayListItem(int atIndex)
{
    if (atIndex >= 0 && atIndex < playListItems.size()) {
        deletePlayListItem(playListItems.getObjects()[atIndex].get(), false);
    }
}

bool PlayListContainer::deleteAssociatedItems(const AudioRegion* audioRegion)
{
    bool success = false;
    for (int i = static_cast<int>(playListItems.size() - 1); i >= 0; i--) {
        if (playListItems.getObjects()[i] != nullptr &&
            playListItems.getObjects()[i]->getRegion().get() == audioRegion)
        {
            deletePlayListItem(i);
            success = true;
        }
    }
    
    return success;
}

bool PlayListContainer::exitsInPlayList(const AudioRegion* region)
{
    auto it = std::find_if(playListItems.getObjects().begin(), playListItems.getObjects().end(), [region](const auto& item) {
        return item->getRegion().get() == region;
    });
    
    return it != playListItems.getObjects().end();
}

const std::vector<std::shared_ptr<PlayListItem>> &PlayListContainer::getPlayListItems() const
{
    return playListItems.getObjects();
}

int PlayListContainer::getNumItems(std::shared_ptr<AudioTrack> track) const
{
    return static_cast<int>(playListItems.size());
}

std::shared_ptr<PlayListItem> PlayListContainer::getPlayListItem(int index) const
{
    auto i = static_cast<size_t> (index);
    if (i >= 0 && i < playListItems.size())
        return playListItems.getObjects()[i];
    return nullptr;
}

int PlayListContainer::getPlayListItemIndex(PlayListItem* item) const
{
    return playListItems.getIndex(std::dynamic_pointer_cast<const PlayListItem>(item->getSharedPtr()));
}

bool PlayListContainer::writeToJson (json& output)
{
    jassert(sortedByPosition());
    json playList;
    
    for (auto item : playListItems.getObjects())
    {
        json j;
        if (item->writeToJson(j))
            output["play_list_vector"] += j;
    }
    
    return true;
}

bool PlayListContainer::readFromJson (json& input, bool rebuild)
{
    json jsonPlayListItems;
    if (input.contains("play_list_vector")) {
        jsonPlayListItems = input["play_list_vector"];
    }
    else {
        // TODO: remove legacy code soon-ish
        auto jsonPlayList = input["play_list"];
        jsonPlayListItems = jsonPlayList["play_list_items"];
    }
    
    const auto numNeeded = jsonPlayListItems.size();
    playListItems.resize (std::min (numNeeded, playListItems.size()));
    while (numNeeded > playListItems.size()) {
        playListItems.objects.emplace_back (std::make_shared<PlayListItem>(*this,
                                                                           nullptr,
                                                                           selectionManager));
    }
    
    for (auto i = 0; i < playListItems.size(); ++i) {
        auto &jsonElement = jsonPlayListItems[i];
        if ( !playListItems.getObjects()[i]->readFromJson(jsonElement, rebuild)) {
            std::cout << "error: could not load play list item:" << std::endl;
            std::cout << jsonElement.dump(4) << std::endl;
        }
    }
    
    jassert(sortedByPosition());
    return true;
}

void PlayListContainer::mergeFromJson(json& input)
{
    json jsonPlayListItems;
    if (input.contains("play_list_vector")) {
        jsonPlayListItems = input["play_list_vector"];
    }
    else {
        // TODO: remove legacy code soon-ish
        auto jsonPlayList = input["play_list"];
        jsonPlayListItems = jsonPlayList["play_list_items"];
    }
    
    for (auto& jsonElement : jsonPlayListItems) {
        
        if (auto playListItem = createPlayListItemFromJson(jsonElement)) {
            jsonElement["track_id"] = playListItem->getRegion()->getAudioTrack()->getId();
            if (playListItem->readFromJson(jsonElement, false)) {
                
                if (!playListItemExists(playListItem))
                    playListItems.push_back(playListItem);
                else
                    std::cout << "playListItemExists" << std::endl;
            }
        }
    }
    jassert(sortedByPosition());
}


bool PlayListContainer::playListItemExists(std::shared_ptr<PlayListItem> other) const
{
    for (auto item : playListItems.getObjects()) {
        if (item->getAbsolutePositionRange(audium::clocks) == other->getAbsolutePositionRange(audium::clocks) &&
            item->getRegion() == other->getRegion()) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItemFromJson (json& input)
{
    std::shared_ptr<PlayListItem> playListItem = nullptr;
    
    auto regionId = input["region_id"].template get<int>();
    
    auto resourceGroupId = 0;
    if (input.contains("sub_group_id")) {
        resourceGroupId = input["sub_group_id"].template get<int>(); // lecacy
    }
    else if (input.contains("resource_group_id")) {
        resourceGroupId = input["resource_group_id"].template get<int>();
    }
    
    if (audioTrack.resourceGroupContainer->objectExistsAtIndex(resourceGroupId)) {
        auto resourceGroup = audioTrack.resourceGroupContainer->getObjects()[resourceGroupId];
        jassert(resourceGroup);
        if (auto audioRegion = resourceGroup->getAudioRegionContainer()->getRegion(regionId)) {
            playListItem = std::shared_ptr<PlayListItem> (new PlayListItem(*this,
                                                                           audioRegion,
                                                                           audioTrack.getSelectionManager()));
            playListItem->readFromJson(input, true);
        }
    }
    
    return playListItem;
}

int PlayListContainer::getSizeInUnits()
{
    return getNumItems() * 2;
}


double PlayListContainer::getAbsolueStartTimeByOrder(const PlayListItem* playListItem, audium::TimeContextType context) const
{
    double startTime = 0.0;
    
    for (auto item : playListItems.getObjects())
    {
        if (item.get() == playListItem)
        {
            return startTime;
        }
        startTime += item->getRegionData(context).getLength();
    }
    
    return startTime;
}

void PlayListContainer::forcePositionByOrder()
{
    for (auto item : playListItems.getObjects())
    {
        item->setAbsolutePosition(getAbsolueStartTimeByOrder(item.get(), audium::clocks), audium::clocks);
    }
}

void PlayListContainer::sortByPosition()
{
    std::sort(playListItems.objects.begin(), playListItems.objects.end(),
              [](const std::shared_ptr<PlayListItem> i1, const std::shared_ptr<PlayListItem> i2)
              {
        return (i1->getAbsolutePosition(audium::clocks) < i2->getAbsolutePosition(audium::clocks));
    });
    jassert(sortedByPosition());
}

bool PlayListContainer::sortedByPosition() const
{
    auto position = 0.0;
    for (auto item : playListItems.getObjects()) {
        auto itemPos = item->getAbsolutePosition(audium::clocks);
        if (itemPos < position)
            return false;
        
        position = itemPos;
    }
    
    return true;
}

double PlayListContainer::findNextFreePosition(double position, audium::TimeContextType context) const
{
    auto &pos = position;
    while (auto item = itemAtAbsolutePosition(pos, context)) {
        pos = item->getAbsolutePositionRange(context).getEnd();
    }
    
    return pos;
}


PlayListItem* PlayListContainer::itemAtAbsolutePosition(double position, audium::TimeContextType context) const
{
    for (auto item : playListItems.getObjects())
    {
        auto startTime = item->getAbsolutePosition(context);
        auto endTime = startTime + item->getDurationTime(context);
        juce::Range<double> absoluteRange(startTime, endTime);
        if (absoluteRange.contains(position))
        {
            return item.get();
        }
    }
    
    return nullptr;
}

PlayListItem* PlayListContainer::itemIntersectingRange(juce::Range<double> range, audium::TimeContextType context) const
{
    for (auto item : playListItems.getObjects()) {
        auto absoluteRange = item->getAbsolutePositionRange(context);
        if (absoluteRange.intersects(range))
            return item.get();
    }
    
    return nullptr;
}

PlayListItem* PlayListContainer::itemContainingRange(juce::Range<double> range, audium::TimeContextType context) const
{
    for (auto item : playListItems.getObjects()) {
        auto absoluteRange = item->getAbsolutePositionRange(context);
        if (absoluteRange.contains(range))
            return item.get();
    }
    
    return nullptr;
}


const std::vector<PlayListItem*> PlayListContainer::itemsAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const
{
    std::vector<PlayListItem*> result;
    
    for (auto item : playListItems.getObjects()) {
        auto absoluteRange = item->getAbsolutePositionRange(context);
        if (absoluteRange.contains(range))
            result.push_back(item.get());
    }
    
    return result;
}

void PlayListContainer::selectPlayListItemWithRegion(std::shared_ptr<AudioRegion> region)
{
    for (auto item : playListItems.getObjects())
    {
        if (item->getRegion() == region)
        {
            item->setSelected(true);
        }
    }
}

double PlayListContainer::getTotalLength(audium::TimeContextType context) const
{
    auto totalLength = 0.0;
    for (auto item : playListItems.getObjects())
    {
        totalLength = juce::jmax(totalLength, item->getAbsolutePositionRange(context).getEnd());
    }
    return totalLength;
}


std::vector<std::shared_ptr<PlayListItem>> PlayListContainer::getSelectedItems(bool global) const
{
    std::vector<std::shared_ptr<PlayListItem>> result;
    
    if (global) {
        auto selectedObjects = selectionManager->getSelectedObjects();
        for (auto object : selectedObjects) {
            if (auto playListItem = std::dynamic_pointer_cast<PlayListItem>(object)) {
                jassert(playListItem->isSelected());
                result.push_back(playListItem);
            }
        }
    }
    else {
        
        for (auto item : playListItems.objects) {
            if (item->isSelected())
                result.push_back(item);
        }
    }
    return result;
}

} // namespace audium
