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

void WaveFormTableListBoxModel::paintRowBackground (juce::Graphics& g,
                                 int rowNumber,
                                 int width, int height,
                                 bool rowIsSelected)
{
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
    if (columnId == 1)
    {
        if (existingComponentToUpdate == nullptr)
        {
            auto component = new WaveFormComponent(zoomHandler);
            component->setAudioResource(audioResourceContainer->getAudioResource(rowNumber));
            zoomHandler->updateTotalLength();
            return component;
        }
        else
        {
            auto component = dynamic_cast<WaveFormComponent*>(existingComponentToUpdate);
            jassert(component);
            zoomHandler->updateTotalLength();
            return component;
        }
    }
    
    return nullptr;
}





