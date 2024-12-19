/*
  ==============================================================================

    PlayListContainer.cpp
    Created: 28 Jun 2023 11:50:56am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioRegionAdapter.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"

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
    if (items.size() > 0)
    {
        insertIndex = getPlayListItemIndex(items.back()) + 1;
    }
    //std::cout << "insert index: " << insertIndex << std::endl;
    
    if (auto playListItem = createPlayListItem(audioRegion, insertIndex))
    {
        playListItem->setAbsolutePosition(position, context);
        sortByPosition();
        return playListItem;
    }
    
    return nullptr;
}

std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItemUI(std::shared_ptr<AudioRegion> region, int insertIndex)
{
    jassert(region);
    auto playListItem = createPlayListItem(region, insertIndex);
    movePlayListItemsPosition(insertIndex);
    return playListItem;
    
}

std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItemsUI(std::vector<std::shared_ptr<AudioRegion>> regions,
                                                   int indexOfItemToPlaceBefore)
{
    std::shared_ptr<PlayListItem> playListItem = nullptr;
    for (auto region : regions) {
        playListItem = createPlayListItem(region, indexOfItemToPlaceBefore);
        movePlayListItemsPosition(indexOfItemToPlaceBefore);
        indexOfItemToPlaceBefore++;
    }
    
    return playListItem;
}


std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItem(int regionIndex, int insertIndex)
{
    auto region = audioRegionContainer.getRegion(regionIndex);
    jassert(region != nullptr);
    return createPlayListItem(region, insertIndex);
}

std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItem(std::shared_ptr<AudioRegion> audioRegion,
                                                                    int insertIndex)
{
    jassert( insertIndex >= 0);
    jassert( insertIndex <= playListItems.size());
    
    auto itemBefore = getPlayListItem(insertIndex - 1);
    
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*this,
                                                                       audioRegion,
                                                                       audioRegion->getAudioTrack()->getSelectionManager()));
    playListItems.objects.insert(playListItems.objects.begin() + insertIndex, playListItem);
    
    auto pos = itemBefore ? itemBefore->getAbsolutePositionRange(audium::clocks).getEnd() : 0.0;
    playListItem->setAbsolutePosition(pos, audium::clocks);
    
    return playListItem;
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
                range = (*iter)->getAbsolutePositionRange(context);
            }
            else {
                break;
            }
        }
    }
}

void PlayListContainer::movePlayListItemBefore(int currentIndex, int indexOfItemToPlaceBefore)
{
    // TODO: swap position
//    auto currentPos = getPlayListItem(currentIndex)->getAbsolutePosition(audium::clocks);
//    auto beforePos = getPlayListItem(indexOfItemToPlaceBefore)->getAbsolutePosition(audium::clocks);
//
//    getPlayListItem(currentIndex)->setAbsolutePosition(beforePos, audium::clocks);
//    getPlayListItem(indexOfItemToPlaceBefore)->setAbsolutePosition(currentPos, audium::clocks);
    
    MoveItemBefore(playListItems.objects,
                   currentIndex,
                   indexOfItemToPlaceBefore);
}

bool PlayListContainer::deletePlayListItem(PlayListItem* playListItem) {
    return playListItems.deleteObject(playListItem);
}

void PlayListContainer::deletePlayListItem(int atIndex, bool sendNotification)
{
    if (atIndex >= 0 && atIndex < playListItems.size())
    {
        deletePlayListItem(playListItems.getObjects()[atIndex].get());
        
        if (sendNotification)
            audioRegionContainer.getAudioTrackContainer().sendActionMessage(playListDeletedAction);
    }
}

bool PlayListContainer::deleteAssociatedItems(const AudioRegion* audioRegion)
{
    bool success = false;
    for (int i = static_cast<int>(playListItems.size() - 1); i >= 0; i--)
    {
        if (playListItems.getObjects()[i]->getRegion().get() == audioRegion)
        {
            deletePlayListItem(i, false);
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
    if (index >= 0 && index < playListItems.size())
    {
        return playListItems.getObjects()[index];
    }
    return nullptr;
}

int PlayListContainer::getPlayListItemIndex(PlayListItem* item) const
{
    return playListItems.getIndex(std::dynamic_pointer_cast<const PlayListItem>(item->getSharedPtr()));
}

bool PlayListContainer::writeToJson (json& output)
{
    json playList;
    
    for (auto item : playListItems.getObjects())
    {
        json j;
        if (item->writeToJson(j))
            playList["play_list_items"] += j;
    }
    
    output["play_list"] = playList;
    return true;
}

bool PlayListContainer::readFromJson (json& input, bool rebuild)
{
    playListItems.cleanup();
    
    auto jsonPlayList = input["play_list"];
    auto jsonPlayListItems = jsonPlayList["play_list_items"];

    for (auto& jsonElement : jsonPlayListItems)
    {
        if (createPlayListItemFromJson(jsonElement) == nullptr)
            return false;
    }
    return true;
}

std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItemFromJson (json& jsonElement)
{
    auto regionIndex = jsonElement["region_id"].template get<int>();
    auto audioRegion = audioRegionContainer.getRegion(regionIndex);
    if (audioRegion != nullptr)
    {
        auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*this,
                                                                           audioRegion,
                                                                           audioRegion->getAudioTrack()->getSelectionManager()));
        playListItems.push_back(playListItem);
        if (playListItem->readFromJson(jsonElement, false))
            return playListItem;
    }
    
    return nullptr;
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

PlayListItem* PlayListContainer::itemAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const
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

