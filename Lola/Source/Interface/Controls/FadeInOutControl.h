//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
    
    void setPlayListItem(std::shared_ptr<PlayListItem> playListItem_) { playListItem = playListItem_; }
    
private:
    FadeType type;
    
    std::shared_ptr<PlayListItem> playListItem;
    std::shared_ptr<RegionSelector> regionSelector;    

    juce::Rectangle<int> originalBounds;
    
    int visualSize      = 8;
    int controlHeight   = 16;
    int controlWidth    = 8;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutControl)
};
