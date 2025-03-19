//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Region/AudioRegionContainer.h"


class TrackRegionTableListBoxModel  : public juce::TableListBoxModel
{
public:
    TrackRegionTableListBoxModel(std::shared_ptr<juce::TableListBox> owner,
                                 std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~TrackRegionTableListBoxModel() override;

    int getNumRows() override;
    
    void paintRowBackground (juce::Graphics&,
                                     int rowNumber,
                                     int width, int height,
                                     bool rowIsSelected) override
    {
    }

    void paintCell (juce::Graphics&,
                            int rowNumber,
                            int columnId,
                            int width, int height,
                            bool rowIsSelected) override
    {
    }
    
    juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override;
    
    void selectedRowsChanged (int lastRowSelected) override;
    
    void deleteKeyPressed (int lastRowSelected) override;
    
    void backgroundClicked (const juce::MouseEvent&) override;
    
    void cellClicked (int rowNumber, int columnId, const juce::MouseEvent&) override;
    
    juce::var getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows) override;

private:
    
    std::shared_ptr<audium::AudioRegion> getRegion(int rowNumber, int columnId) const;
    
    std::shared_ptr<juce::TableListBox> owner;
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackRegionTableListBoxModel)
};
