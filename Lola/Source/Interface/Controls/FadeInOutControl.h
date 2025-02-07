/*
  ==============================================================================

    FadeInOutControl.h
    Created: 6 Feb 2025 4:57:47pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


#include "Engine/PlayList/PlayListItem.h"
#include "Interface/Controls/RegionSelector.h"

class FadeInOutControl  : public juce::Component
{
public:
    enum FadeType {
        FadeIn,
        FadeOut
    };
    
    FadeInOutControl(FadeType type_,
                     std::shared_ptr<PlayListItem> playListItem_,
                     std::shared_ptr<RegionSelector> regionSelector_) :
        type(type_),
        playListItem(playListItem_),
        regionSelector(regionSelector_)
    {
    }
    
    ~FadeInOutControl() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;

    /** You can assign a lambda to this callback object to have it called when the slider value is changed. */
    std::function<void()> onValueChange;

    /** You can assign a lambda to this callback object to have it called when the slider's drag begins. */
    std::function<void()> onDragStart;

    /** You can assign a lambda to this callback object to have it called when the slider's drag ends. */
    std::function<void()> onDragEnd;
    
    double getValue() const;
    void setValue(double val);
    
private:
    FadeType type;
    
    std::shared_ptr<PlayListItem> playListItem;
    std::shared_ptr<RegionSelector> regionSelector;    

    juce::Rectangle<int> originalBounds;
    
    int visualSize = 10;
    int controlSize = 15;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutControl)
};
