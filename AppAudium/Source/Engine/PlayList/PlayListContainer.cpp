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

void PlayListContainer::createPlayListItem(std::shared_ptr<AudioRegion> audioRegion)
{
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(audioRegion));
    playListItems.push_back(playListItem);
    
    //sendActionMessage(regionCreatedAction);
}

void PlayListContainer::createPlayListItem(int regionIndex, int indexOfItemToPlaceBefore)
{
    jassert( indexOfItemToPlaceBefore >= 0);
    jassert( indexOfItemToPlaceBefore <= playListItems.size());
    
    auto region = audioRegionContainer->getRegion(regionIndex);
    jassert(region != nullptr);
    
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(region));
    playListItems.insert(playListItems.begin() + indexOfItemToPlaceBefore, playListItem);
    
    //sendActionMessage(regionCreatedAction);
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
        if (getPlayListItem(i)->getRegion() == audioRegion)
        {
            deletePlayListItem(i, false);
        }
    }
}

std::shared_ptr<PlayListItem> PlayListContainer::getPlayListItem(int index) const
{
    if (index >= 0 && index < playListItems.size())
    {
        return playListItems[index];
    }
    return nullptr;
}

AudioRegion::RegionData PlayListContainer::getPlayListDataAtIndex(int index) const
{
    const juce::ScopedLock sl (readLock);
    if (index >= 0 && index < playListItems.size())
    {
        return playListItems[index]->getRegionData();
    }
    
    // returns empty range
    return AudioRegion::RegionData();
}

bool PlayListContainer::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(static_cast<int>(playListItems.size()));
    for (auto & item : playListItems)
    {
        outputStream.writeInt(audioRegionContainer->getRegionIndex(item->getRegion()));
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
            auto audioRegion    = audioRegionContainer->getRegion(regionIndex);
            jassert(regionName == audioRegion->name);
            auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(audioRegion));
            playListItems.push_back(playListItem);
        }
        return true;
    }
    return false;
}
