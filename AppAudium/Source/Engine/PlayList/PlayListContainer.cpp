/*
  ==============================================================================

    PlayListContainer.cpp
    Created: 28 Jun 2023 11:50:56am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudioRegion.h"
#include "Engine/AudioRegionContainer.h"
#include "Engine/ActionMessages.h"

PlayListContainer::~PlayListContainer()
{
    playListItems.clear();
}

void PlayListContainer::createPlayListItem(std::shared_ptr<AudioRegion> audioRegion)
{
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*this, audioRegion));
    playListItems.push_back(playListItem);
    
    sendActionMessage(playListItemCreatedAction);
}

void PlayListContainer::createPlayListItem(int regionIndex, int indexOfItemToPlaceBefore)
{
    jassert( indexOfItemToPlaceBefore >= 0);
    jassert( indexOfItemToPlaceBefore <= playListItems.size());
    
    auto region = audioRegionContainer.getRegion(regionIndex);
    jassert(region != nullptr);
    
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*this, region));
    playListItems.insert(playListItems.begin() + indexOfItemToPlaceBefore, playListItem);
    
    sendActionMessage(playListItemCreatedAction);
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

void PlayListContainer::deleteAssociatedItems(std::shared_ptr<AudioRegion> audioRegion)
{
    for (int i = static_cast<int>(playListItems.size() - 1); i >= 0; i--)
    {
        if (playListItems[i]->getRegion() == audioRegion)
        {
            deletePlayListItem(i, false);
        }
    }
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

AudioRegion::RegionData PlayListContainer::getPlayListDataAtIndex(int index) const
{
    const juce::ScopedLock sl (readLock);
    if (index >= 0 && index < playListItems.size())
    {
        return playListItems[index]->getRegionData();
    }
    
    // empty range
    return AudioRegion::RegionData();
}

bool PlayListContainer::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(static_cast<int>(playListItems.size()));
    for (auto & item : playListItems)
    {
        outputStream.writeInt(audioRegionContainer.getRegionIndex(item->getRegion()));
        outputStream.writeString(item->getRegion()->name);
    }
    return true;
}

bool PlayListContainer::readFromStream (juce::InputStream& inputStream)
{
    if (!inputStream.isExhausted())
    {
        playListItems.clear();
        auto numItems = inputStream.readInt();
        for (auto i = 0; i < numItems; i++)
        {
            auto regionIndex    = inputStream.readInt();
            auto regionName     = inputStream.readString();
            auto audioRegion    = audioRegionContainer.getRegion(regionIndex);
            if (audioRegion != nullptr)
            {
                jassert(regionName == audioRegion->name);
                auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*this, audioRegion));
                playListItems.push_back(playListItem);
            }
            else
            {
                jassertfalse;
                return false;
            }
        }
        return true;
    }
    return false;
}

double PlayListContainer::getAbsolueStartTime(const PlayListItem* playListItem) const
{
    double startTime = 0.0;
    auto group = playListItem->getRegion()->getAudioGroup();
    
    auto items = getPlayListItems();
    
    for (auto & item : items)
    {
        if (item.get() == playListItem)
        {
            return startTime;
        }
        startTime += item->getRegionData().getLength();
    }
    
    return startTime;
}

const PlayListItem* PlayListContainer::itemAtAbsolutePosition(double position) const
{
    for (auto item : playListItems)
    {
        auto startTime = item->getAbsolueStartTime();
        auto endTime = startTime + item->getDurationTime();
        juce::Range<double> absoluteRange(startTime, endTime);
        if (absoluteRange.contains(position))
        {
            return item.get();
        }
    }
    
    return nullptr;
}

const PlayListItem* PlayListContainer::itemAtAbsoluteRange(juce::Range<double> range) const
{
    for (auto item : playListItems)
    {
        auto startTime = item->getAbsolueStartTime();
        auto endTime = startTime + item->getDurationTime();
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

void PlayListContainer::deselectAll()
{
    for (auto item : playListItems)
    {
        item->setSelected(false);
    }
    
    sendActionMessage(playListItemSelection);
}

double PlayListContainer::getTotalLength() const
{
    if (playListItems.size() > 0)
    {
        auto lastItem = playListItems.back();
        return getAbsolueStartTime(lastItem.get()) + lastItem->getDurationTime();
    }
    return 0.0;
}
