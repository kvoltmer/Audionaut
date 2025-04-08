//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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
                     std::shared_ptr<audium::PlayListItem> playListItem_,
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
    
    void setPlayListItem(std::shared_ptr<audium::PlayListItem> playListItem_) { playListItem = playListItem_; }
    
private:
    FadeType type;
    
    std::shared_ptr<audium::PlayListItem> playListItem;
    std::shared_ptr<RegionSelector> regionSelector;    

    juce::Rectangle<int> originalBounds;
    
    int visualSize      = 8;
    int controlHeight   = 16;
    int controlWidth    = 8;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutControl)
};
