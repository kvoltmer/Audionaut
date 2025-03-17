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
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Channel/AudioChannel.h"

class SubGroupListBoxModel  : public audium::ListBoxModel
{
public:
    SubGroupListBoxModel(std::shared_ptr<audium::ListBox> owner,
                         std::shared_ptr<AudioSubGroup> audioSubGroup,
                         std::shared_ptr<AudiumEngine> audiumEngine,
                         std::shared_ptr<ZoomHandler> zoomHandler,
                         std::shared_ptr<RegionSelector> regionSelector) :
        owner(owner),
        audioSubGroup(audioSubGroup),
        audiumEngine(audiumEngine),
        zoomHandler(zoomHandler),
        regionSelector(regionSelector)
    {
    }
    
    ~SubGroupListBoxModel() override
    {
    }
    
    int getNumRows() override
    {
        // std::cout << audioSubGroup->getNumChannels() << std::endl;
        return audioSubGroup->getNumChannels();
    }

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override
    {
        auto channel = audioSubGroup->getAudioTrack().getChannel(rowNumber);
        
        if (channel != nullptr &&
            channel->isSelected())
        {
            g.setColour (owner->findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.9f));
            g.fillRect(Rectangle<int>(0, 0, width, height));
        }
        
    }
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
    
        auto audioResource = audioSubGroup->getAudioResourceAtChannel(rowNumber);
        if (existingComponentToUpdate == nullptr)
        {
            if (audioResource != nullptr)
            {
                auto component = new AudioResourceView(owner.get(),
                                                       audiumEngine,
                                                       audioResource,
                                                       zoomHandler,
                                                       audioSubGroup->getAudioTrack().getColour(),
                                                       regionSelector,
                                                       rowNumber);
                return component;
            }
            else
            {
                return new juce::Component();
            }
        }
        else
        {
            auto component = dynamic_cast<AudioResourceView*>(existingComponentToUpdate);
            if (component != nullptr)
            {
                component->updateUI(rowNumber);
                return component;
            }
            else
            {
                return existingComponentToUpdate;
            }
        }
        
        return nullptr;
    }

    
    int getRowHeight (int rowNumber) const override
    {
        auto channel = audioSubGroup->getAudioTrack().getChannel(rowNumber);
        if (channel != nullptr)
        {
            return channel->getChannelHeight();
        }
        else
        {
            return 100;
        }
    }
    
    void deleteKeyPressed (int lastRowSelected) override
    {
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        owner->deselectAllRows();
    }
    
    void listWasScrolled() override {}
    
    void selectedRowsChanged (int lastRowSelected) override
    {
    }
    
    void setAudioSubGroup(std::shared_ptr<AudioSubGroup> subGroup)
    {
        audioSubGroup = subGroup;
    }
    
        
private:
    
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubGroupListBoxModel)
};
