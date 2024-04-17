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
#include "Engine/Group/AudioRegionAdapter.h"
#include "Engine/Group/AudioGroupContainer.h"

PlayListContainer::~PlayListContainer()
{
    cleanup();
}

bool PlayListContainer::createPlayListItemUI(int rowNumber, int insertIndex)
{
    // convert row number to region index
    auto regions = audioRegionContainer.getAudioGroupContainer().getAudioRegionAdapter().getAudioRegions();
    jassert(rowNumber < regions.size());
    auto region = regions[rowNumber];
    
    // check if region exists in conainer
    if (audioRegionContainer.getRegionIndex(region) >= 0)
    {
        createPlayListItem(region, insertIndex);
        
        // move to the left
        auto length = getPlayListItem(insertIndex)->getRegionData(audium::clocks).getLength();
        movePlayListItemsPosition(insertIndex + 1, length, audium::clocks);
        
        return true;
    }
    return false;
}


void PlayListContainer::createPlayListItem(int regionIndex, int insertIndex)
{
    auto region = audioRegionContainer.getRegion(regionIndex);
    jassert(region != nullptr);
    createPlayListItem(region, insertIndex);
}

void PlayListContainer::createPlayListItem(std::shared_ptr<AudioRegion> audioRegion, int insertIndex)
{
    jassert( insertIndex >= 0);
    jassert( insertIndex <= playListItems.size());
    
    auto itemBefore = getPlayListItem(insertIndex - 1);
    
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*this, audioRegion));
    playListItems.insert(playListItems.begin() + insertIndex, playListItem);
    
    auto pos = itemBefore ? itemBefore->getAbsolutePositionRange(audium::clocks).getEnd() : 0.0;
    playListItem->setAbsolutePosition(pos, audium::clocks);
    
    //sendActionMessage(playListItemCreatedAction);
}



void PlayListContainer::movePlayListItemsPosition(int startIndex, double amount, audium::TimeContextType context)
{
    for (auto iter = playListItems.begin() + startIndex; iter != playListItems.end(); iter++)
    {
        (*iter)->moveAbsolutePosition(amount, context);
    }
}

void PlayListContainer::movePlayListItemBefore(int currentIndex, int indexOfItemToPlaceBefore)
{
    MoveItemBefore(playListItems,
                   currentIndex,
                   indexOfItemToPlaceBefore);
}

void PlayListContainer::deletePlayListItem(int atIndex, bool sendNotification)
{
    if (atIndex >= 0 && atIndex < playListItems.size())
    {
        playListItems.erase(playListItems.begin() + atIndex);
        if (sendNotification)
            sendActionMessage(playListDeletedAction);
    }
}

bool PlayListContainer::deleteAssociatedItems(const AudioRegion* audioRegion)
{
    bool success = false;
    for (int i = static_cast<int>(playListItems.size() - 1); i >= 0; i--)
    {
        if (playListItems[i]->getRegion().get() == audioRegion)
        {
            deletePlayListItem(i, false);
            success = true;
        }
    }
    return success;
}

const std::vector<std::shared_ptr<PlayListItem>> PlayListContainer::getPlayListItems() const
{
    return playListItems;
}

int PlayListContainer::getNumItems(std::shared_ptr<AudioGroup> group) const
{
    return static_cast<int>(playListItems.size());
}

std::shared_ptr<PlayListItem> PlayListContainer::getPlayListItem(int index) const
{
    if (index >= 0 && index < playListItems.size())
    {
        return playListItems[index];
    }
    return nullptr;
}

int PlayListContainer::getPlayListItemIndex(const PlayListItem* item) const
{
    for (auto i = 0; i < playListItems.size(); i++)
    {
        if (playListItems[i].get() == item)
            return i;
    }
    
    return -1;
}

AudioRegionData::tRange PlayListContainer::getPlayListDataAtIndex(int index) const
{
    const juce::ScopedLock sl (readLock);
    if (index >= 0 && index < playListItems.size())
    {
        return playListItems[index]->getRegionData(audium::clocks);
    }
    
    // empty range
    return AudioRegionData::tRange();
}

bool PlayListContainer::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool PlayListContainer::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        sendActionMessage(playListOrderAction);
        return true;
    }
    return false;
}

bool PlayListContainer::writeToJson (json& output)
{
    json playList;
    
    for (auto item : playListItems)
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
    if (rebuild)
        cleanup();
    
    auto jsonPlayList = input["play_list"];
    auto jsonPlayListItems = jsonPlayList["play_list_items"];
    auto i = 0;
    for (auto& jsonElement : jsonPlayListItems)
    {
        auto regionIndex = jsonElement["region_id"].template get<int>();
        auto audioRegion = audioRegionContainer.getRegion(regionIndex);
        if (audioRegion != nullptr)
        {
            std::shared_ptr<PlayListItem> playListItem = nullptr;
            if (rebuild)
            {
                playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*this, audioRegion));
                playListItems.push_back(playListItem);
            }
            else
            {
                playListItem = playListItems[i];
            }
            playListItem->readFromJson(jsonElement, rebuild);
        }
        else
        {
            jassertfalse;
            return false;
        }
        i++;
    }
    return true;
}

int PlayListContainer::getSizeInUnits()
{
    return getNumItems() * 2;
}


double PlayListContainer::getAbsolueStartTimeByOrder(const PlayListItem* playListItem, audium::TimeContextType context) const
{
    double startTime = 0.0;
        
    for (auto item : playListItems)
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
    for (auto item : playListItems)
    {
        item->setAbsolutePosition(getAbsolueStartTimeByOrder(item.get(), audium::clocks), audium::clocks);
    }
}

const PlayListItem* PlayListContainer::itemAtAbsolutePosition(double position, audium::TimeContextType context) const
{
    for (auto item : playListItems)
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

const PlayListItem* PlayListContainer::itemAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const
{
    for (auto item : playListItems)
    {
        auto startTime = item->getAbsolutePosition(context);
        auto endTime = startTime + item->getDurationTime(context);
        juce::Range<double> absoluteRange(startTime, endTime);
        if (absoluteRange.contains(range))
        {
            return item.get();
        }
    }
    
    return nullptr;
}

void PlayListContainer::selectPlayListItem(std::shared_ptr<PlayListItem> item, bool bSelected)
{
    item->setSelected(bSelected);
    
    sendActionMessage(playListItemSelection);
}

double PlayListContainer::getTotalLength(audium::TimeContextType context) const
{
    if (playListItems.size() > 0)
    {
        return playListItems.back()->getAbsolutePositionRange(context).getEnd();
    }
    return 0.0;
}

void PlayListContainer::deleteSelectedItems()
{
    auto selected = getSelectedRows();
    for (int i = selected.size()-1; i >= 0; i--)
    {
        auto item = getPlayListItem(selected[i]);
        jassert(item);
        deletePlayListItem(selected[i]);
    }
    
}

void PlayListContainer::deselectAll()
{
    for (auto item : playListItems)
    {
        item->setSelected(false);
    }
    //sendActionMessage(playListItemSelection);
}

juce::SparseSet<int> PlayListContainer::getSelectedRows() const
{
    juce::SparseSet<int> result;
    for (auto i = 0; i < getNumItems(); i++)
    {
        if (getPlayListItem(i) != nullptr &&
            getPlayListItem(i)->isSelected())
        {
            result.addRange ({i, i + 1});
        }
    }
    return result;
}

void PlayListContainer::setSelectedRows(juce::SparseSet<int>& selectedRows)
{
    deselectAll();
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (auto item = getPlayListItem(selectedRows[i]))
        {
            item->setSelected(true);
        }
    }
}
