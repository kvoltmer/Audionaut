/*
  ==============================================================================

    WaveFormListBoxModel.cpp
    Created: 2 Feb 2023 5:15:07pm
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "WaveFormTableListBoxModel.h"

WaveFormTableListBoxModel::WaveFormTableListBoxModel(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                                                     std::shared_ptr<ZoomHandler> zoomHandler) :
    audioResourceContainer(audioResourceContainer),
    zoomHandler(zoomHandler)
{
}

WaveFormTableListBoxModel::~WaveFormTableListBoxModel()
{
}

int WaveFormTableListBoxModel::getNumRows()
{
    return audioResourceContainer->getAudioResourceSize();
}

void WaveFormTableListBoxModel::paintListBoxItem ( int rowNumber,
                        juce::Graphics& g,
                        int width, int height,
                        bool rowIsSelected)
{
    
}

juce::Component* WaveFormTableListBoxModel::refreshComponentForRow (int rowNumber, bool isRowSelected,
                                                                     juce::Component* existingComponentToUpdate)
{

    if (existingComponentToUpdate == nullptr)
    {
        auto audioResource = audioResourceContainer->getAudioResource(rowNumber);
        if (audioResource != nullptr)
        {
            auto component = new WaveFormComponent(zoomHandler);
            component->setAudioResource(audioResource);
            return component;
        }
    }
    else
    {
        auto component = dynamic_cast<WaveFormComponent*>(existingComponentToUpdate);
        jassert(component);
        return component;
    }
    
    
    return nullptr;
}





