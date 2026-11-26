//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <vector>

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/MiddlePanel/ArrangementView/AudioTrackComponent.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Controls/AudioTrackListBox.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelGroupComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/Models/TrackListBoxModels.h"

class ChannelGroupListBoxModel : public TrackRowsListBoxModel {
    
public:
    
    ChannelGroupListBoxModel(std::shared_ptr<audium::ListBox> owner,
                              std::shared_ptr<audium::AudiumEngine> audiumEngine) :
        TrackRowsListBoxModel(audiumEngine),
        owner(owner)
    {
    }

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            auto thumbArea = juce::Rectangle<int>(0, 0, width, height);
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

    void backgroundClicked (const juce::MouseEvent&) override
    {
        audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
        owner->deselectAllRows();
        // updateAll, as a header click: the arrangement rows, the mixer and the right panel's selected-track header follow the deselect
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateAll);
    }
        
private:
    std::shared_ptr<audium::ListBox> owner;

};
