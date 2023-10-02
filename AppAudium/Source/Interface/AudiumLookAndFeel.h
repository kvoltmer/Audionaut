/*
  ==============================================================================

    AudiumLookAndFeel.h
    Created: 7 Jun 2023 3:56:54pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ColourIds.h"

class AudiumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AudiumLookAndFeel();
    ~AudiumLookAndFeel() override;
    
    static LookAndFeel_V4::ColourScheme getDarkAudiumColourScheme();
    
private:
    void setupColours();
};
