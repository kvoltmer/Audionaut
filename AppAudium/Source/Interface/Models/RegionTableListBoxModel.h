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

//==============================================================================
/*
*/
class RegionTableListBoxModel  : public juce::TableListBoxModel
{
public:
    RegionTableListBoxModel(std::shared_ptr<AudioRegionContainer> audioRegionContainer);
    ~RegionTableListBoxModel() override;

    //==============================================================================
    /** This must return the number of rows currently in the table.

        If the number of rows changes, you must call TableListBox::updateContent() to
        cause it to refresh the list.
    */
    int getNumRows() override;

    /** This must draw the background behind one of the rows in the table.

        The graphics context has its origin at the row's top-left, and your method
        should fill the area specified by the width and height parameters.

        Note that the rowNumber value may be greater than the number of rows in your
        list, so be careful that you don't assume it's less than getNumRows().
    */
    void paintRowBackground (juce::Graphics&,
                                     int rowNumber,
                                     int width, int height,
                                     bool rowIsSelected) override;

    /** This must draw one of the cells.

        The graphics context's origin will already be set to the top-left of the cell,
        whose size is specified by (width, height).

        Note that the rowNumber value may be greater than the number of rows in your
        list, so be careful that you don't assume it's less than getNumRows().
    */
    void paintCell (juce::Graphics&,
                            int rowNumber,
                            int columnId,
                            int width, int height,
                            bool rowIsSelected) override;
private:
    
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionTableListBoxModel)
};
