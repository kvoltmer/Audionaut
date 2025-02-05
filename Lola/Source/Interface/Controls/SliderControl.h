

#pragma once

#include <JuceHeader.h>
#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Controls/RegionSelector.h"

class SliderControl  : public juce::Slider
{
public:
    SliderControl (const juce::String& componentName,
                   std::shared_ptr<RegionSelector> regionSelector_) :
        juce::Slider(componentName),
        regionSelector(regionSelector_)
    {
    }
    
    ~SliderControl() = default;
    
    void mouseEnter (const MouseEvent& e)
    {
        if (regionSelector != nullptr)
            regionSelector->setEnabled(false);
    }

    void mouseExit (const MouseEvent& e)
    {
        if (regionSelector != nullptr)
            regionSelector->setEnabled(true);
    }

private:
    
    std::shared_ptr<RegionSelector> regionSelector;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliderControl)
};
