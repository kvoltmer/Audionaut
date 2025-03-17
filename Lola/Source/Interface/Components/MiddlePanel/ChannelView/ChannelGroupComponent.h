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

#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Models/ChannelSubGroupListBoxModel.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelGroupHeaderComponent.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"

//==============================================================================
/*
    Display a track and it's channels
 
*/
class ChannelGroupComponent  : public juce::Component
{
public:
    ChannelGroupComponent(std::shared_ptr<audium::AudioTrack> audioTrack,
                          std::shared_ptr<audium::AudiumEngine> audiumEngine) :
        audioTrack(audioTrack),
        audiumEngine(audiumEngine)
    {
        channelsListBox.reset(new audium::ListBox());
        
        auto header = std::unique_ptr<ChannelGroupHeaderComponent>(new ChannelGroupHeaderComponent(audioTrack));
        channelsListBox->setHeaderComponent(std::move(header));
        channelsListBox->getHeaderComponent()->setSize(getWidth(), DraggerControl::draggerHeight);
        channelsListBox->setOutlineThickness(0);
        channelsListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(channelsListBox.get());
        
        channelsListBoxModel.reset(new ChannelSubGroupListBoxModel(*channelsListBox.get(), audiumEngine, audioTrack));
        channelsListBox->setModel(channelsListBoxModel.get());
        
    }

    ~ChannelGroupComponent() override
    {
        audioTrack = nullptr;
        channelsListBox->setModel(nullptr);
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override
    {
        channelsListBox->setBounds(getLocalBounds());
    }
    
    void refreshComponent(std::shared_ptr<audium::AudioTrack> newAudioTrack)
    {
        if (audioTrack != newAudioTrack) {
            channelsListBoxModel->setAudioTrack(newAudioTrack);
            audioTrack = newAudioTrack;
        }
        channelsListBox->updateContent();
        channelsListBox->setSelectedRows(audioTrack->audioChannelContainer->getSelectedRows(), dontSendNotification);
        
        // the audio track title
        auto headerComponent = dynamic_cast<ChannelGroupHeaderComponent*>( channelsListBox->getHeaderComponent() );
        if (headerComponent != nullptr)
            headerComponent->updateFromEngine(audioTrack);
    }

private:
    
    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    std::unique_ptr<audium::ListBox> channelsListBox;
    std::unique_ptr<ChannelSubGroupListBoxModel> channelsListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupComponent)
};
