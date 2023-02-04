/*
  ==============================================================================

    WaveFormListBoxModel.cpp
    Created: 2 Feb 2023 5:15:07pm
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "WaveFormTableListBoxModel.h"

WaveFormTableListBoxModel::WaveFormTableListBoxModel(std::shared_ptr<AudioResourceContainer> audioResourceContainer) :
    audioResourceContainer(audioResourceContainer),
    zoomFactor(0.0)
{
}

WaveFormTableListBoxModel::~WaveFormTableListBoxModel()
{
}

int WaveFormTableListBoxModel::getNumRows()
{
    return audioResourceContainer->getAudioResourceSize();
}

void WaveFormTableListBoxModel::paintRowBackground (juce::Graphics& g,
                                 int rowNumber,
                                 int width, int height,
                                 bool rowIsSelected)
{
    g.fillAll (juce::Colour (0x00000000));
}

void WaveFormTableListBoxModel::paintCell (juce::Graphics&,
                        int rowNumber,
                        int columnId,
                        int width, int height,
                        bool rowIsSelected)
{
}

juce::Component* WaveFormTableListBoxModel::refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                                     juce::Component* existingComponentToUpdate)
{
    std::cout << "refreshComponentForCell row " << rowNumber << " col " << columnId <<  std::endl;

    if (columnId == 1)
    {
        if (existingComponentToUpdate == nullptr)
        {
            auto component = new WaveFormComponent(transportSource);
            component->setAudioResource(audioResourceContainer->getAudioResource(rowNumber));
            //component->setZoomFactor(zoomFactor);
            waveFormComponentCache.push_back(component);
            return component;
        }
        else
        {
            auto component = dynamic_cast<WaveFormComponent*>(existingComponentToUpdate);
            component->setZoomFactor(zoomFactor);
            
            return existingComponentToUpdate;
        }
    }
    
    return nullptr;
}
