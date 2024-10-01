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
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Undo/UndoableContainerAction.h"

#include "Interface/Controls/PlayListTableListBox.h"
#include "Interface/ColourIds.h"
#include "Interface/Models/PlayListTableListBoxItem.h"

class PlayListTableListBoxModel : public juce::TableListBoxModel {
    
public:
    PlayListTableListBoxModel(std::shared_ptr<PlayListTableListBox> listBox,
                              std::shared_ptr<AudiumEngine> engine,
                              std::shared_ptr<AudioGroup> group) :
        listBox(listBox),
        audiumEngine(engine),
        audioGroup(group)
    {
    }
    
    int getNumRows() override
    {
        return audiumEngine->getPlayListContainer(audioGroup)->getNumItems();
    }

    void paintRowBackground (juce::Graphics& g,
                                     int rowNumber,
                                     int width, int height,
                                     bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            g.fillAll (listBox->findColour(audium::listBoxBackgroundColourId));
        }
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
//            auto items = audiumEngine->getPlayListContainer(audioGroup)->getPlayListItems();
//            const PlayListItem* const p = items[rowNumber].get();
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

    void deleteKeyPressed (int lastRowSelected) override
    {
        // Undo: store old state
        auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioGroupContainer());
        
        auto selected = listBox->getSelectedRows();

        for (int i = selected.size()-1; i >= 0; i--)
        {
            audiumEngine->getPlayListContainer(audioGroup)->deletePlayListItem(selected[i]);
        }
        
        // Undo: store new state
        action->storeNewState();
        audiumEngine->getUndoManager()->perform(action.release(), "Delete PlayList Item(s)");
        audiumEngine->getUndoManager()->beginNewTransaction();
    }
    
    juce::var getDragSourceDescription (const juce::SparseSet< int > &rowsToDescribe) override
    {
        return "PlayListTableListBoxModel";
    }
    
    void selectedRowsChanged (int lastRowSelected) override
    {
        auto selectedRows = listBox->getSelectedRows();
        audioGroup->getPlayListContainer()->setSelectedRows(selectedRows);
        // only update transport (updateAll would mess with the keyboard focus)
        audioGroup->getAudioGroupContainer().sendActionMessage(updateMiddlePanelAction);
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        listBox->deselectAllRows();
        audioGroup->getPlayListContainer()->selectAllItems(false);
        audioGroup->getAudioGroupContainer().sendActionMessage(updateAll);
    }

    
    std::shared_ptr<PlayListTableListBox> listBox;
    
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return audiumEngine->getPlayListContainer(audioGroup); }
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return audiumEngine->getPlayListScheduler(); }
    
    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    
private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioGroup> audioGroup;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListTableListBoxModel)
};
