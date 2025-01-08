/*
  ==============================================================================

    RegionContainerModel.h
    Created: 7 Jun 2023 2:01:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudiumEngine;
class AudioTrack;

//==============================================================================
/*
*/

class RegionContainerModel  : public juce::TableListBoxModel {
public:
    RegionContainerModel(std::shared_ptr<juce::TableListBox> owner,
                         std::shared_ptr<AudiumEngine> audiumEngine,
                         std::shared_ptr<AudioTrack> audioTrack);
    ~RegionContainerModel() override;

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
    
    std::shared_ptr<juce::TableListBox> owner;
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioTrack> audioTrack;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionContainerModel)
};
