/*
  ==============================================================================

    TempoProvider.h
    Created: 14 Nov 2023 4:09:49pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

#include "Engine/Link/LinkEngine.hpp"
#include "Engine/ActionMessages.h"

// Provide and store the current tempo
class TempoProvider : public juce::ActionBroadcaster
{
public:
    
    TempoProvider(std::shared_ptr<audium::LinkEngine> linkEngine) :
        linkEngine(linkEngine)
    {
        linkEngine->getLink()->setTempoCallback([this](const double p) { onTempoChange(p); });
        linkEngine->getLink()->enable(false);
        linkEngine->setTempo(tempoBPM);
    }
    
    void setTempo(double newTempo)
    {
        tempoBPM = newTempo;
        if (linkEngine != nullptr)
        {
            linkEngine->setTempo(newTempo);
        }
        
        sendActionMessage (tempoChanged);
    }
    
    double getTempo() const
    {
        return tempoBPM;
    }
    
    void onTempoChange(double newTempo)
    {
        tempoBPM = newTempo;
        sendActionMessage (tempoChanged);
    }

    
private:
    std::shared_ptr<audium::LinkEngine> linkEngine;
    
    double tempoBPM = 120.0;
    
};
