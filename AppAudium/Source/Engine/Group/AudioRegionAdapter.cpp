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
    auto regions = getAudioRegions();
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (auto region = regions[selectedRows[i]])
        {
            region->setSelected(true);
        }
    }
}

void AudioRegionAdapter::deleteSelectedRegions()
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(owner.getptr());
  
    auto selectedRegions = getSelectedAudioRegions();
    
    for (auto region : selectedRegions)
    {
        region->getAudioGroup()->getAudioRegionContainer()->deleteAudioRegion(region);
    }
    
    // Undo: store new state
    action->storeNewState();
    owner.getUndoManager()->perform(action.release(), "Delete Region(s)");
    owner.getUndoManager()->beginNewTransaction();
}
