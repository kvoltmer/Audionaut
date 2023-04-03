/*
  ==============================================================================

    WaveFormListBoxModel.h
    Created: 2 Feb 2023 5:15:07pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <vector>

#include <JuceHeader.h>
#include "Engine/AudioResourceContainer.h"
#include "Interface/Components/WaveFormComponent.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"

class WaveFormTableListBoxModel : public audium::ListBoxModel
{
    
    
public:
    
    WaveFormTableListBoxModel(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                              std::shared_ptr<ZoomHandler> zoomHandler);
    ~WaveFormTableListBoxModel();
    
    int getNumRows() override;

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override;
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override;

    
    int getRowHeight (int rowNumber) const override;
    
    void selectedRowsChanged (int lastRowSelected) override;
    
    void deleteKeyPressed (int lastRowSelected) override;
        
private:
    
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
    std::shared_ptr<ZoomHandler> zoomHandler;

};
