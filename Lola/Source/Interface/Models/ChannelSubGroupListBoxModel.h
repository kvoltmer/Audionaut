/*
  ==============================================================================

    ChannelSubGroupListBoxModel.h
    Created: 23 Oct 2023 3:14:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioGroupContainer.h"

class ChannelSubGroupListBoxModel  : public audium::ListBoxModel
{
public:
    ChannelSubGroupListBoxModel(audium::ListBox& owner,
                                std::shared_ptr<AudiumEngine> audiumEngine,
                                std::shared_ptr<AudioGroup> audioGroup) :
        owner(owner),
        audiumEngine(audiumEngine),
        audioGroup(audioGroup)
    {
    }
    
    ~ChannelSubGroupListBoxModel() override
    {
    }
    
    int getNumRows() override
    {
        return audioGroup->getNumChannels();
    }

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override
    {
    }
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
        auto channel = audioGroup->getChannel(rowNumber);
        if (channel != nullptr)
        {
            if (existingComponentToUpdate == nullptr)
            {
                return new ChannelComponent(audioGroup, audiumEngine, rowNumber);
                
            }
            else
            {
                auto component = dynamic_cast<ChannelComponent*>(existingComponentToUpdate);
                jassert(component);
                
                if (component != nullptr)
                {
                    // update of audioGroup since row might have changed after delete
                    component->refreshComponent(audioGroup, rowNumber, isRowSelected);
                }
                return component;
            }
        }
        
        
        return nullptr;
    }

    
    int getRowHeight (int rowNumber) const override
    {
        auto channel = audioGroup->getChannel(rowNumber);
        if (channel != nullptr)
            return audioGroup->getChannel(rowNumber)->getChannelHeight();
        
        return 50;
    }
    
    void deleteKeyPressed (int lastRowSelected) override
    {
        audiumEngine->getAudioGroupContainer()->deleteSelectedObjects();
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        owner.deselectAllRows();
    }
    
    void listWasScrolled() override {}
    
    void selectedRowsChanged (int lastRowSelected) override
    {
        auto selectedRows = owner.getSelectedRows();
        audioGroup->setSelectedRows(selectedRows);
        audiumEngine->getAudioGroupContainer()->sendActionMessage(updateArrangementAction);
    }
    
    void setAudioGroup(std::shared_ptr<AudioGroup> group)
    {
        audioGroup = group;
    }
        
private:
    audium::ListBox& owner;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioGroup> audioGroup;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelSubGroupListBoxModel)
};
