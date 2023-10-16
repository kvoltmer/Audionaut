/*
  ==============================================================================

    RegionTableListBoxModel.h
    Created: 7 Jun 2023 2:01:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Controls/RegionTableListBox.h"

class PlayListContainer;
class AudioRegionContainer;

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
                            std::shared_ptr<AudioRegionContainer> audioRegionContainer);
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
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        owner->deselectAllRows();
    }
    
    juce::var getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows) override;

private:
    
    std::shared_ptr<RegionTableListBox> owner;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionTableListBoxModel)
};
