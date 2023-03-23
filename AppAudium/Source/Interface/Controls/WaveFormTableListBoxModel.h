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

class WaveFormTableListBoxModel : public juce::TableListBoxModel
{
    
    
public:
    
    WaveFormTableListBoxModel(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                              std::shared_ptr<ZoomHandler> zoomHandler);
    ~WaveFormTableListBoxModel();
    
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
    
        
    
private:
    
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
    std::shared_ptr<ZoomHandler> zoomHandler;

};
