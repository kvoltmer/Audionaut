
#pragma once
#include <vector>

#include <JuceHeader.h>
#include "Engine/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/MiddlePanel/ArrangementView/ArrangementGroupComponent.h"
#include "Interface/Components/MiddlePanel/EditView/EditGroupComponent.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelGroupComponent.h"

#include "Engine/Group/AudioGroupContainer.h"

class ChannelGroupListBoxModel : public audium::ListBoxModel {
    
public:
    
    ChannelGroupListBoxModel(std::shared_ptr<audium::ListBox> owner,
                              std::shared_ptr<AudiumEngine> audiumEngine) :
        owner(owner),
        audiumEngine(audiumEngine)
    {
    }
    
    ~ChannelGroupListBoxModel() override
    {
    }
    
    int getNumRows() override
    {
        return audiumEngine->getAudioGroupContainer()->getNumItems();
    }

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            auto thumbArea = Rectangle<int>(0, 0, width, height);
            g.setColour (Colours::lightgrey);
            g.drawRoundedRectangle (thumbArea.toFloat(), 3.0f, 2.0f);
        }
    }
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
        auto audioGroup = audiumEngine->getAudioGroupContainer()->getAudioGroup(rowNumber);
        if (existingComponentToUpdate == nullptr)
        {
            if (audioGroup != nullptr)
            {
                return new ChannelGroupComponent(audioGroup, audiumEngine);
            }
        }
        else
        {
            auto component = dynamic_cast<ChannelGroupComponent*>(existingComponentToUpdate);
            jassert(component);
        
            if (audioGroup != nullptr)
            {
                // update of audioGroup since row might have changed after delete
                component->refreshComponent(audioGroup);
            }
            return component;
        }
        
        
        return nullptr;
    }

    
    int getRowHeight (int rowNumber) const override
    {
        auto group = audiumEngine->getAudioGroupContainer()->getAudioGroup(rowNumber);
        if (group != nullptr)
            return group->getTotalHeight() + DraggerControl::draggerHeight;
        
        jassertfalse;
        return 0;
    }
    
    void deleteKeyPressed (int lastRowSelected) override
    {
        auto selected = owner->getSelectedRows();
        auto audioGroupContainer = audiumEngine->getAudioGroupContainer();
        for (int i = selected.size()-1; i >= 0; i--)
        {
            std::cout << "delete selected = " << selected[i] << std::endl;
            auto group = audioGroupContainer->getAudioGroup(selected[i]);
            if (group != nullptr)
            {
                
                audioGroupContainer->deleteAudioGroup(group);
            }
            else
            {
                jassertfalse;
            }
        }
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        owner->deselectAllRows();
    }
    
    void listWasScrolled() override
    {
    }
    
    void selectedRowsChanged (int lastRowSelected) override
    {
        std::cout << "selectedRowsChanged" << lastRowSelected << std::endl;
    }
        
private:
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<AudiumEngine> audiumEngine;

};
