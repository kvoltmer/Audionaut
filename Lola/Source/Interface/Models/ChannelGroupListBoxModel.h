//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <vector>

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/MiddlePanel/ArrangementView/AudioTrackComponent.h"
#include "Interface/Components/MiddlePanel/EditView/AudioTrackRegionEditComponent.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Controls/AudioTrackListBox.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelGroupComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Group/AudioTrackContainer.h"

class ChannelGroupListBoxModel : public audium::ListBoxModel {
    
public:
    
    ChannelGroupListBoxModel(std::shared_ptr<audium::ListBox> owner,
                              std::shared_ptr<audium::AudiumEngine> audiumEngine) :
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
        if (auto audioTrack = audiumEngine->getAudioTrackContainer()->getAudioTrack(rowNumber)) {
            
            if (existingComponentToUpdate == nullptr) {
                return new ChannelGroupComponent(audioTrack, audiumEngine);
            }
            else {
                auto component = dynamic_cast<ChannelGroupComponent*>(existingComponentToUpdate);
                jassert(component);
                // update of audioTrack since row might have changed after delete
                component->refreshComponent(audioTrack);
                return component;
            }
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
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateMiddlePanelAction);
    }
    
    void listWasScrolled() override
    {
    }
    
    void selectedRowsChanged (int lastRowSelected) override
    {
    }
    
    int getExtraSpaceAtBottom() const override
    {
        return AudiumLookAndFeel::extraSpaceAtBottom;
    }
        
private:
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;

};
