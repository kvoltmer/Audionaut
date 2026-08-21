//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>


#include "Engine/PlayList/PlayListItem.h"
#include "Interface/Controls/RegionSelector.h"

class DraggingHandle  : public juce::Component
{
public:
    enum FadeType {
        FadeIn,         // fade-in ramp end, top strip
        FadeOut,        // fade-out ramp start, top strip
        FadeInStart,    // fade-in ramp start, bottom edge
        FadeOutEnd,     // fade-out ramp end, bottom edge
        CurveIn,        // fade-in bend handle on the ramp midpoint, vertical drag
        CurveOut        // fade-out bend handle on the ramp midpoint, vertical drag
    };

    DraggingHandle(FadeType type_,
                     std::shared_ptr<audium::PlayListItem> playListItem_,
                     std::shared_ptr<RegionSelector> regionSelector_) :
        type(type_),
        playListItem(playListItem_),
        regionSelector(regionSelector_)
    {
        setMouseCursor(isCurveType() ? juce::MouseCursor::UpDownResizeCursor
                                     : juce::MouseCursor::LeftRightResizeCursor);
    }
    
    ~DraggingHandle() override = default;

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

    /** Parent-relative y the bottom-edge handles (FadeInStart/FadeOutEnd) sit
        on - the bottom of the first channel row. 0 falls back to the parent
        bottom. */
    void setBottomAnchorY(int y) { bottomAnchorY = y; }

    /** The clip's x-range in the parent's coordinates. When set, the handle is
        parented to the track lane instead of the clip and its value maps
        against this range - positions outside it yield negative values (fade
        extending outside the clip). For the curve types this is the RAMP's
        x-range instead - the handle sits on its midpoint. Empty range =
        legacy parent-is-the-clip mode. */
    void setClipRange(juce::Range<int> range) { clipRange = range; }

    bool isCurveType() const { return type == CurveIn || type == CurveOut; }

    /** The component a mouseExit is forwarded to (the clip component). Used
        instead of getParentComponent() so lane-parented handles still notify
        their clip. */
    void setExitTarget(juce::Component* target) { exitTarget = target; }

private:
    FadeType type;
    
    std::shared_ptr<audium::PlayListItem> playListItem;
    std::shared_ptr<RegionSelector> regionSelector;    

    juce::Rectangle<int> originalBounds;
    
    int visualSize      = 8;
    int controlHeight   = 16;
    int controlWidth    = 8;
    int bottomAnchorY   = 0;

    juce::Range<int> clipRange;
    juce::Component* exitTarget = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggingHandle)
};
