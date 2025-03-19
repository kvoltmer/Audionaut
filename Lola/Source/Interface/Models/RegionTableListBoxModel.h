//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Controls/RegionTableListBox.h"

namespace audium {
    class PlayListContainer;
    class AudioRegionContainer;
    class AudioTrackContainer;
}
enum RegionColumns {
    regionName      = 1,
    regionStart     = 2,
    regionEnd       = 3,
    regionLength    = 4
};

//==============================================================================
/*
*/

class RegionTableListBoxModel  : public juce::TableListBoxModel {
public:
    RegionTableListBoxModel(std::shared_ptr<RegionTableListBox> owner,
                            std::shared_ptr<audium::AudioTrackContainer> audioTrackContainer);
    ~RegionTableListBoxModel() override;

    int getNumRows() override;

    void paintRowBackground (juce::Graphics&,
                                     int rowNumber,
                                     int width, int height,
                                     bool rowIsSelected) override;

    void paintCell (juce::Graphics&,
                            int rowNumber,
                            int columnId,
                            int width, int height,
                            bool rowIsSelected) override;
    
    juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override;
    
    void selectedRowsChanged (int lastRowSelected) override;
    
    void deleteKeyPressed (int lastRowSelected) override;
    
    void backgroundClicked (const juce::MouseEvent&) override;
    
    juce::var getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows) override;

private:
    
    std::shared_ptr<RegionTableListBox> owner;
    std::shared_ptr<audium::AudioTrackContainer> audioTrackContainer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionTableListBoxModel)
};
