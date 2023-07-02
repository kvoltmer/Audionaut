/*
  ==============================================================================

    PlayListTableListBoxModel.h
    Created: 28 Jun 2023 1:57:11pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/AudioRegion.h"
#include "Interface/Controls/PlayListTableListBox.h"
#include "Interface/ColourIds.h"
#include "Interface/Models/PlayListTableListBoxItem.h"

class PlayListTableListBoxModel : public juce::TableListBoxModel {
    
public:
    PlayListTableListBoxModel(std::shared_ptr<PlayListTableListBox> listBox,
                              std::shared_ptr<PlayListContainer> playListContainer) :
        listBox(listBox),
        playListContainer(playListContainer)
    {
    }
    
    int getNumRows() override
    {
        return playListContainer->getNumItems();
    }

    void paintRowBackground (juce::Graphics& g,
                                     int rowNumber,
                                     int width, int height,
                                     bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll (listBox->findColour(audium::defaultHighlightColourId));
    }

    void paintCell (juce::Graphics& g,
                            int rowNumber,
                            int columnId,
                            int width, int height,
                            bool rowIsSelected) override
    {
    }

    juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
        if (existingComponentToUpdate == nullptr)
        {
            if (const PlayListItem* const p = playListContainer->getPlayListItem(rowNumber).get())
            {
                return new PlayListTableListBoxItem(this, columnId, rowNumber);
            }
        }
        else
        {
            auto component = dynamic_cast<PlayListTableListBoxItem*>(existingComponentToUpdate);
            if (component != nullptr)
            {
                component->update(columnId, rowNumber, isRowSelected);
                return component;
            }
        }
        
        return nullptr;
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        //audioRegionContainer->setSelectedRegion(lastRowSelected);
    }

    void deleteKeyPressed (int lastRowSelected) override
    {
        auto selected = listBox->getSelectedRows();

        for (int i = selected.size()-1; i >= 0; i--)
        {
            playListContainer->deletePlayListItem(selected[i]);
        }
    }
    
    juce::var getDragSourceDescription (const juce::SparseSet< int > &rowsToDescribe) override
    {
        return "PlayListTableListBoxModel";
    }

    
    std::shared_ptr<PlayListTableListBox> listBox;

    std::shared_ptr<PlayListContainer> playListContainer;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListTableListBoxModel)
};
