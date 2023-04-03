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
    if (rowIsSelected)
    {
        auto thumbArea = Rectangle<int>(0, 0, width, height);
        g.setColour (Colours::yellow);
        g.drawRoundedRectangle (thumbArea.toFloat(), 3.0f, 1.0f);
    }
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

int WaveFormTableListBoxModel::getRowHeight (int rowNumber) const
{
    if (rowNumber < audioResourceContainer->getAudioResourceSize())
    {
        auto audioResource = audioResourceContainer->getAudioResource(rowNumber);
        return audioResource->height;
    }
    jassertfalse;
    return 0;
}

void WaveFormTableListBoxModel::selectedRowsChanged (int lastRowSelected)
{
    std::cout << "selectedRowsChanged: " << lastRowSelected << std::endl;
}

void WaveFormTableListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    std::cout << "deleteKeyPressed: " << lastRowSelected << std::endl;
}

