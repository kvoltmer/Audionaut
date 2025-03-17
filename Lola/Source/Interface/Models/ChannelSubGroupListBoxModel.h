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

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrackContainer.h"

class ChannelSubGroupListBoxModel  : public audium::ListBoxModel
{
public:
    ChannelSubGroupListBoxModel(audium::ListBox& owner,
                                std::shared_ptr<AudiumEngine> audiumEngine,
                                std::shared_ptr<AudioTrack> audioTrack) :
        owner(owner),
        audiumEngine(audiumEngine),
        audioTrack(audioTrack)
    {
    }
    
    ~ChannelSubGroupListBoxModel() override
    {
    }
    
    int getNumRows() override
    {
        return audioTrack->getNumAudioTrackChannels();
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
        if (auto channel = audioTrack->getChannel(rowNumber)) {
            if (existingComponentToUpdate == nullptr) {
                return new ChannelComponent(audioTrack, audiumEngine, rowNumber);
            }
            else {
                auto component = dynamic_cast<ChannelComponent*>(existingComponentToUpdate);
                jassert(component);
                
                if (component != nullptr)
                {
                    // update of audioTrack since row might have changed after delete
                    component->refreshComponent(audioTrack, rowNumber, isRowSelected);
                }
                return component;
            }
        }
        
        
        return nullptr;
    }

    
    int getRowHeight (int rowNumber) const override
    {
        auto channel = audioTrack->getChannel(rowNumber);
        if (channel != nullptr)
            return audioTrack->getChannel(rowNumber)->getChannelHeight();
        
        return 50;
    }
    
    void deleteKeyPressed (int lastRowSelected) override
    {
        audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        owner.deselectAllRows();
    }
    
    void listWasScrolled() override {}
    
    void selectedRowsChanged (int lastRowSelected) override
    {
        auto selectedRows = owner.getSelectedRows();
        audioTrack->audioChannelContainer->setSelectedRows(selectedRows);
        audiumEngine->getAudioTrackContainer()->sendActionMessage(updateArrangementAction);
    }
    
    void setAudioTrack(std::shared_ptr<AudioTrack> track)
    {
        audioTrack = track;
    }
        
private:
    audium::ListBox& owner;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioTrack> audioTrack;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelSubGroupListBoxModel)
};
