
#pragma once
#include <vector>

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/MiddlePanel/ArrangementView/AudioTrackComponent.h"
#include "Interface/Components/MiddlePanel/EditView/EditGroupComponent.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Controls/AudioTrackListBox.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelGroupComponent.h"

#include "Engine/Group/AudioTrackContainer.h"

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
        return audiumEngine->getAudioTrackContainer()->getNumItems();
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
        auto audioTrack = audiumEngine->getAudioTrackContainer()->getAudioTrack(rowNumber);
        if (existingComponentToUpdate == nullptr)
        {
            if (audioTrack != nullptr)
            {
                return new ChannelGroupComponent(audioTrack, audiumEngine);
            }
        }
        else
        {
            auto component = dynamic_cast<ChannelGroupComponent*>(existingComponentToUpdate);
            jassert(component);
        
            if (audioTrack != nullptr)
            {
                // update of audioTrack since row might have changed after delete
                component->refreshComponent(audioTrack);
            }
            return component;
        }
        
        
        return nullptr;
    }

    
    int getRowHeight (int rowNumber) const override
    {
        auto track = audiumEngine->getAudioTrackContainer()->getAudioTrack(rowNumber);
        if (track != nullptr)
            return track->getTotalHeight() + DraggerControl::draggerHeight;
        
        return 0;
    }
    
    void deleteKeyPressed (int lastRowSelected) override
    {
        audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
        owner->deselectAllRows();
        audiumEngine->getAudioTrackContainer()->sendActionMessage(updateMiddlePanelAction);
    }
    
    void listWasScrolled() override
    {
    }
    
    void selectedRowsChanged (int lastRowSelected) override
    {
    }
        
private:
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<AudiumEngine> audiumEngine;

};
