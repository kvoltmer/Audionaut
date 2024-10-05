/*
  ==============================================================================

    AudioRegionAdapter.cpp
    Created: 15 Apr 2024 11:05:43am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioRegionAdapter.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Resource/AudioResourceContainer.h"

AudioRegionAdapter::AudioRegionAdapter(AudioGroupContainer &owner) :
    owner(owner)
{
}

const std::vector<std::shared_ptr<AudioRegion>> AudioRegionAdapter::getAudioRegions() const
{
    std::vector<std::shared_ptr<AudioRegion>> result;
    
    for (auto g = 0; g < owner.getNumItems(); g++)
    {
        auto group = owner.getAudioGroup(g);
        for (auto i = 0; i < group->getAudioRegionContainer()->getNumRegions() ; i++)
        {
            result.push_back(group->getAudioRegionContainer()->getRegion(i));
        }
    }
    
    return result;
}

std::shared_ptr<AudioRegion> AudioRegionAdapter::getRegion(int rowNumber) const
{
    auto regions = getAudioRegions();
    if (regions.size() > 0 && rowNumber < regions.size())
        return regions[rowNumber];
    
    return nullptr;
}

const std::vector<std::shared_ptr<AudioRegion>> AudioRegionAdapter::getSelectedAudioRegions() const
{
    std::vector<std::shared_ptr<AudioRegion>> result;
    
    auto regions = getAudioRegions();
    
    for (auto region : regions)
    {
        if (region->isSelected())
            result.push_back(region);
    }
    
    return result;
}

void AudioRegionAdapter::deselectAll()
{
    auto audioRegions = getAudioRegions();
    for (auto region : audioRegions)
    {
        region->setSelected(false);
    }
}

juce::SparseSet<int> AudioRegionAdapter::getSelectedRows() const
{
    
    juce::SparseSet<int> selection;
    auto regions = getAudioRegions();
    for (auto i = 0; i < regions.size(); i++)
    {
        if (regions[i]->isSelected())
            selection.addRange ({i, i + 1});
    }
    
    return selection;
}

void AudioRegionAdapter::setSelectedRows(juce::SparseSet<int>& selectedRows)
{
    deselectAll();
    
    // de-select all play list items
    for (auto i = 0; i < owner.getNumItems(); i++)
    {
        if (auto group = owner.getAudioGroup(i))
        {
            group->getPlayListContainer()->selectAllItems(false);
        }
    }
    
    auto regions = getAudioRegions();
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (selectedRows[i] < regions.size())
        {
            if (auto region = regions[selectedRows[i]])
            {
                region->setSelected(true);
                
                // select associated play list items
                for (auto i = 0; i < owner.getNumItems(); i++)
                {
                    if (auto group = owner.getAudioGroup(i))
                    {
                        group->getPlayListContainer()->selectPlayListItemWithRegion(region);
                    }
                }
            }
        }
        else
        {
            jassertfalse;
        }
    }
}

void AudioRegionAdapter::createRegionsFromSelection(juce::String name, bool arrangementMode)
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(owner);
    
    for (auto i = 0; i < owner.getNumItems(); i++)
    {
        if (auto group = owner.getAudioGroup(i))
        {
            
            if (arrangementMode)
            {
                if (auto item = group->getPlayListContainer()->itemAtAbsoluteRange(selectedPositionClocks, audium::clocks))
                {
                    // we need the start of the actual audio file
                    auto localStart = selectedPositionClocks.getStart() - item->getAbsolutePosition(audium::clocks) + item->getRegionData(audium::clocks).getStart();
                    
                    juce::Range<double> localRange(localStart, localStart + selectedPositionClocks.getLength());
                    auto localRangeInSeconds = owner.getTempoProvider()->clocksToSeconds(localRange);
                    group->getAudioRegionContainer()->createRegion(name, localRangeInSeconds, group, item->getRegion()->getAudioSubGroup());
                }
            }
            else
            {
                // get resources at this range
                auto rangeInSeconds = owner.getTempoProvider()->clocksToSeconds(selectedPositionClocks);
                // TODO: change this to subgroups
                auto resources = group->getAudioResourcesAtAbsoluteRange(rangeInSeconds);
                if (resources.size() > 0)
                {
                    // grab the first valid resource
                    auto resource  = resources[0];
                    
                    auto maxLength = 0.0;
                    for (auto res : resources)
                        maxLength = std::max(maxLength, res->getFileLength(audium::seconds));
                    
                    const auto transportPosition = resource->getAudioSubGroup()->getAudioClip()->getAbsolutePosition(audium::seconds);
                    rangeInSeconds -= transportPosition;
                    const auto startPosition = resource->getAudioSubGroup()->getAudioClip()->getRegionData(audium::seconds).getStart();
                    rangeInSeconds += startPosition;
                    
                    if (rangeInSeconds.getEnd() > maxLength)
                        rangeInSeconds.setEnd(maxLength);
                    
                    group->getAudioRegionContainer()->createRegion(name, rangeInSeconds, group, resource->getAudioSubGroup());
                }
            }
        }
    }
    // clear selection
    selectedPositionClocks = juce::Range<double>();
    
    
    // Undo: store new state
    action->storeNewState();
    owner.getUndoManager()->perform(action.release(), "Create Region(s)");
    owner.getUndoManager()->beginNewTransaction();
}

void AudioRegionAdapter::setSelectedPosition(juce::Range<double> pos, audium::TimeContextType context)
{
    if (context == audium::seconds)
    {
        selectedPositionClocks = owner.getTempoProvider()->secondsToClocks(pos);
    }
    else if (context == audium::clocks)
    {
        selectedPositionClocks = pos;
    }
    else
    {
        jassertfalse;
    }
}

juce::Range<double> AudioRegionAdapter::getSelectedPosition(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return owner.getTempoProvider()->clocksToSeconds(selectedPositionClocks);
    }
    else if (context == audium::clocks)
    {
        return selectedPositionClocks;
    }

    jassertfalse;
    return juce::Range<double>();
}
