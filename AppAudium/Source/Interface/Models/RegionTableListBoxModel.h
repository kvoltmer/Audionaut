/*
  ==============================================================================

    RegionTableListBoxModel.h
    Created: 7 Jun 2023 2:01:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudioRegionContainer.h"

class RegionTableListBox;
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
    
    /** To allow rows from your table to be dragged-and-dropped, implement this method.

        If this returns a non-null variant then when the user drags a row, the table will try to
        find a DragAndDropContainer in its parent hierarchy, and will use it to trigger a
        drag-and-drop operation, using this string as the source description, and the listbox
        itself as the source component.

        @see getDragSourceCustomData, DragAndDropContainer::startDragging
    */
    juce::var getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows) override;

private:
    
    std::shared_ptr<RegionTableListBox> owner;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionTableListBoxModel)
};
