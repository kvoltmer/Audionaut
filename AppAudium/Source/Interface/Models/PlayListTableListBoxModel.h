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
#include "Engine/PlayList/PlayListSchedulder.h"
#include "Engine/AudiumEngine.h"
#include "Engine/AudioRegion.h"
#include "Interface/Controls/PlayListTableListBox.h"
#include "Interface/ColourIds.h"
#include "Interface/Models/PlayListTableListBoxItem.h"

class PlayListTableListBoxModel : public juce::TableListBoxModel {
    
public:
    PlayListTableListBoxModel(std::shared_ptr<PlayListTableListBox> listBox,
                              std::shared_ptr<AudiumEngine> engine) :
        listBox(listBox),
        audiumEngine(engine)
    {
    }
    
    int getNumRows() override
    {
        return audiumEngine->getPlayListContainer()->getNumItems();
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
            if (const PlayListItem* const p = audiumEngine->getPlayListContainer()->getPlayListItem(rowNumber).get())
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
            audiumEngine->getPlayListContainer()->deletePlayListItem(selected[i]);
        }
    }
    
    juce::var getDragSourceDescription (const juce::SparseSet< int > &rowsToDescribe) override
    {
        return "PlayListTableListBoxModel";
    }
    
    void cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent&) override
    {
        // this is not being called
        audiumEngine->getPlayListScheduler()->setPlayListItemIndex(rowNumber);
    }

    
    std::shared_ptr<PlayListTableListBox> listBox;
    
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return audiumEngine->getPlayListContainer(); }
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return audiumEngine->getPlayListScheduler(); }
    std::shared_ptr<TransportSourceProvider> getTransportSourceProvider() const { return audiumEngine->getTransportSourceProvider(); }
    
private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListTableListBoxModel)
};
