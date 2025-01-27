/*
  ==============================================================================

    RegionContainerModel.cpp
    Created: 7 Jun 2023 2:01:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>

#include "RegionContainerModel.h"
#include "Interface/ColourIds.h"
#include "Interface/Controls/RegionLabel.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"


RegionContainerModel::RegionContainerModel(std::shared_ptr<juce::TableListBox> owner_,
                     std::shared_ptr<AudiumEngine> audiumEngine_,
                     std::shared_ptr<AudioTrack> audioTrack_) :
    owner(owner_),
    audiumEngine(audiumEngine_),
    audioTrack(audioTrack_)
{
}

RegionContainerModel::~RegionContainerModel()
{
}

int RegionContainerModel::getNumRows()
{
    return audioTrack->getAudioRegionContainer()->getNumRegions();
}

void RegionContainerModel::paintRowBackground (juce::Graphics& g,
                                 int rowNumber,
                                 int width, int height,
                                 bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll (owner->findColour(audium::listBoxBackgroundColourId));
    }
}

void RegionContainerModel::paintCell (juce::Graphics& g,
                        int rowNumber,
                        int columnId,
                        int width, int height,
                        bool rowIsSelected)
{    
}

juce::Component* RegionContainerModel::refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                            juce::Component* existingComponentToUpdate)
{
    auto region = audioTrack->getAudioRegionContainer()->getRegion(rowNumber);
    if (existingComponentToUpdate == nullptr)
    {
        if (region != nullptr)
        {
            return new RegionLabel(audiumEngine->getAudioTrackContainer(),
                                   audioTrack->getAudioRegionContainer(),
                                   columnId,
                                   rowNumber);
        }
    }
    else
    {
        auto component = dynamic_cast<RegionLabel*>(existingComponentToUpdate);
        if (component != nullptr)
        {
            // update since row might have changed after delete
            component->update(columnId, rowNumber, isRowSelected);
            return component;
        }
    }
    
    return nullptr;
}

void RegionContainerModel::selectedRowsChanged (int lastRowSelected)
{
    auto selectedRows = owner->getSelectedRows();
    
    audioTrack->getAudioRegionContainer()->setSelectedRows(selectedRows);
    audiumEngine->getAudioTrackContainer()->sendActionMessage(updateAll);
}

void RegionContainerModel::backgroundClicked (const juce::MouseEvent&)
{
    owner->deselectAllRows();
}

void RegionContainerModel::deleteKeyPressed (int lastRowSelected)
{
    audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
}

juce::var RegionContainerModel::getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows)
{
    return "region";
}

