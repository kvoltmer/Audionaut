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
#include "Engine/AudioRegion.h"
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
            auto items = audiumEngine->getPlayListContainer(audioGroup)->getPlayListItems();
            const PlayListItem* const p = items[rowNumber].get();
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
        auto selected = listBox->getSelectedRows();

        for (int i = selected.size()-1; i >= 0; i--)
        {
            audiumEngine->getPlayListContainer(audioGroup)->deletePlayListItem(selected[i]);
        }
    }
    
    juce::var getDragSourceDescription (const juce::SparseSet< int > &rowsToDescribe) override
    {
        return "PlayListTableListBoxModel";
    }
    
    void cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent&) override
    {
        // this is not being called
        // TODO: implement for all schedulers
        jassertfalse;
        
        audiumEngine->getPlayListScheduler()->setPlayListItemIndex(rowNumber);
    }
    
    void selectedRowsChanged (int lastRowSelected) override
    {
        // selecting a playListItem also selects the region
        auto playListItem = audioGroup->getPlayListContainer()->getPlayListItem(lastRowSelected);
        if (playListItem != nullptr)
        {
            audioGroup->getPlayListContainer()->selectPlayListItem(playListItem, true);
            auto regionIndex = audiumEngine->getAudioRegionContainer()->getRegionIndex(playListItem->getRegion());
            audiumEngine->getAudioRegionContainer()->setSelectedRegion(regionIndex);
        }
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        listBox->deselectAllRows();
        audioGroup->getPlayListContainer()->deselectAll();
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
