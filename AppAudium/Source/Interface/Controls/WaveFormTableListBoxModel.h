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

class WaveFormTableListBoxModel : public juce::TableListBoxModel
{
    
    
public:
    
    WaveFormTableListBoxModel(std::shared_ptr<AudioResourceContainer> audioResourceContainer);
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
    
    void setZoomFactor(double zoom) { zoomFactor = zoom; }
    
private:
    
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
    /// TODO: not sure if we will need this
    std::vector<WaveFormComponent*> waveFormComponentCache;
    
    /// TODO: remove this...
    juce::AudioTransportSource transportSource;
    
    double zoomFactor;
    
};
