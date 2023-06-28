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
#include "Interface/Controls/PlayListTableListBox.h"
#include "Interface/ColourIds.h"

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
        if (auto r = playListContainer->getPlayListItem(rowNumber))
        {
            juce::String text;

            if (columnId == 1)
            {
                text = r->getRegion()->name;
            }
            else if (columnId == 2)
            {
                text = juce::String(r->getRegion()->position.getLength(), 2);
            }

            if (rowIsSelected)
                g.setColour (listBox->findColour (audium::defaultHighlightedTextColourId));
            else
                g.setColour (listBox->findColour (audium::defaultTextColourId));

            g.setFont (13.0f);
            g.drawText (text, 4, 0, width - 6, height, juce::Justification::centredLeft, true);
        }
        
    }

    juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
//        if (existingComponentToUpdate == nullptr)
//        {
//            if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
//            {
//                return new RegionEditor(owner, audioRegionContainer, columnId, rowNumber);
//            }
//        }
//        else
//        {
//            auto component = dynamic_cast<RegionEditor*>(existingComponentToUpdate);
//            if (component != nullptr)
//            {
//                // update since row might have changed after delete
//                component->update(columnId, rowNumber, isRowSelected);
//            }
//            return component;
//
//        }
        
        return nullptr;
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        //audioRegionContainer->setSelectedRegion(lastRowSelected);
    }

    void deleteKeyPressed (int lastRowSelected) override
    {
//        auto selected = owner->getSelectedRows();
//
//        for (int i = selected.size()-1; i >= 0; i--)
//        {
//            audioRegionContainer->deleteRegion(selected[i]);
//        }
    }

private:
    
    
    std::shared_ptr<PlayListTableListBox> listBox;
    std::shared_ptr<PlayListContainer> playListContainer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListTableListBoxModel)
};
