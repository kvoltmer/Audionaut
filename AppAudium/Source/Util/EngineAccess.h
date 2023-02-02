/*
  ==============================================================================

    EngineAccess.h
    Created: 30 Jan 2023 11:17:56am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "MainWindow.h"
#include "Engine/AudiumEngine.h"

/// helper to access the engine from anywhere in UI code
std::shared_ptr<AudiumEngine> getAudiumEngine(juce::Component* component);

