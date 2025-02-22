/*
  ==============================================================================

    AudioRegionAdapter.cpp
    Created: 15 Apr 2024 11:05:43am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioRegionAdapter.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"

AudioRegionAdapter::AudioRegionAdapter(AudioTrackContainer &owner_) :
    owner(owner_)
{
}

const std::vector<std::shared_ptr<AudioRegion>> AudioRegionAdapter::getAudioRegions() const
{
    std::vector<std::shared_ptr<AudioRegion>> result;
    
    for (auto track : owner.getAudioTracks()) {
        for (auto region : track->getRegions()) {
            result.push_back(region);
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
    owner.getSelectionManager()->deselectAll();
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
                    if (auto track = owner.getAudioTrack(i))
                    {
                        track->getPlayListContainer()->selectPlayListItemWithRegion(region);
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
   
    auto context = audium::clocks;
    auto selectedRange = getSelectedRange(context);
    
    for (auto track : owner.getAudioTracks()) {
        if (arrangementMode) {
            auto context = audium::clocks;
            if (auto item = track->getPlayListContainer()->itemAtAbsoluteRange(selectedRange, context)) {
                auto localRange = item->absoluteToLocalRange(selectedRange, context);
                auto subGroup = item->getRegion()->getAudioSubGroup();
                subGroup->getAudioRegionContainer()->createRegion(name,
                                                                   localRange,
                                                                   track,
                                                                   subGroup,
                                                                   item->getRegion(),
                                                                   context);
            }
        }
        else
        {
            if (auto subGroup = track->getSubGroupAtAbsoluteRange(selectedRange, context)) {
                auto localRange = subGroup->absoluteToLocalRange(selectedRange, context);
                subGroup->getAudioRegionContainer()->createRegion(name,
                                                               localRange,
                                                               track,
                                                               subGroup,
                                                               nullptr,
                                                               context);
            }
        }
    }
    
    // Undo: store new state
    action->storeNewState();
    owner.getUndoManager()->perform(action.release(), "Create Region(s)");
    owner.getUndoManager()->beginNewTransaction();
}

void AudioRegionAdapter::splitRegionsFromSelection(bool withUndo)
{
    // Undo: store old state
    auto action = withUndo ? std::make_unique<audium::UndoableContainerAction>(owner) : nullptr;
    auto context = audium::clocks;
    auto selectedRange = getSelectedRange(context);
    juce::String name;
    juce::Range<double> absoluteRange, localRange;
    std::shared_ptr<AudioRegion> region;
    
    for (auto track : owner.getAudioTracks()) {
        if (auto item = track->getPlayListContainer()->itemAtAbsoluteRange(selectedRange, context)) {
            auto subGroup = item->getRegion()->getAudioSubGroup();
            bool success = false;
            auto itemRange = item->getAbsolutePositionRange(context);
        
            // - region of possible start (left of selection)
            if (selectedRange.getStart() - itemRange.getStart() > 0.0) {
                
                absoluteRange = juce::Range<double>(itemRange.getStart(), selectedRange.getStart());
                localRange = item->absoluteToLocalRange(absoluteRange, context);
                
                name = subGroup->getAudioRegionContainer()->getUniqueName(item->getRegion()->getName());
                region = subGroup->getAudioRegionContainer()->createRegion(name,
                                                                           localRange,
                                                                           track,
                                                                           subGroup,
                                                                           item->getRegion(),
                                                                           context);
                track->getPlayListContainer()->createPlayListItemAtPositionUI(region, item->getAbsolutePosition(context), context);
                success = true;
            }
            
            // - region of selection
            if (selectedRange.getLength() > 0.0) {
                localRange = item->absoluteToLocalRange(selectedRange, context);
                name = subGroup->getAudioRegionContainer()->getUniqueName(item->getRegion()->getName());
                region = subGroup->getAudioRegionContainer()->createRegion(name,
                                                                           localRange,
                                                                           track,
                                                                           subGroup,
                                                                           item->getRegion(),
                                                                           context);
                track->getPlayListContainer()->createPlayListItemAtPositionUI(region, selectedRange.getStart(), context);
                success = true;
            }
            
            // - region of possible remainder
            if (itemRange.getEnd() - selectedRange.getEnd() > 0.0) {
                
                absoluteRange = juce::Range<double>(selectedRange.getEnd(), itemRange.getEnd());
                localRange = item->absoluteToLocalRange(absoluteRange, context);
                name = subGroup->getAudioRegionContainer()->getUniqueName(item->getRegion()->getName());
                region = subGroup->getAudioRegionContainer()->createRegion(name,
                                                                           localRange,
                                                                           track,
                                                                           subGroup,
                                                                           item->getRegion(),
                                                                           context);
                track->getPlayListContainer()->createPlayListItemAtPositionUI(region, selectedRange.getEnd(), context);
                success = true;
            }
            
            if (success) {
                track->getPlayListContainer()->deletePlayListItem(item);
                track->getPlayListContainer()->sortByPosition();
            }
        }
    }
    
    // Undo: store new state
    if (action != nullptr) {
        action->storeNewState();
        owner.getUndoManager()->perform(action.release(), "Split Region(s)");
        owner.getUndoManager()->beginNewTransaction();
    }
}

void AudioRegionAdapter::setSelectedRange(juce::Range<double> pos, audium::TimeContextType context)
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

juce::Range<double> AudioRegionAdapter::getSelectedRange(audium::TimeContextType context) const
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

bool AudioRegionAdapter::anyRangeSelected() const
{
    return !selectedPositionClocks.isEmpty();
}
