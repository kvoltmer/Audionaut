//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Channel/AudioChannel.h"

class ChannelSubGroupListBoxModel  : public audium::ListBoxModel
{
public:
    ChannelSubGroupListBoxModel(audium::ListBox& owner,
                                std::shared_ptr<audium::AudiumEngine> audiumEngine,
                                std::shared_ptr<audium::AudioTrack> audioTrack) :
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
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateArrangementAction);
    }
    
    void setAudioTrack(std::shared_ptr<audium::AudioTrack> track)
    {
        audioTrack = track;
    }
        
private:
    audium::ListBox& owner;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<audium::AudioTrack> audioTrack;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelSubGroupListBoxModel)
};
