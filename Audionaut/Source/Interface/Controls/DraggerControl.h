//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/Controls/RegionSelector.h"
#include "Engine/Resource/AudioResource.h"
#include "Interface/ColourIds.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/PlayList/PositionableBase.h"

class DraggerControl  : public juce::Component,
                        public juce::ChangeBroadcaster,
                        public juce::KeyListener
{
public:
    DraggerControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                   std::shared_ptr<ZoomHandler> zoomHandler_,
                   juce::Colour colour_,
                   std::shared_ptr<RegionSelector> regionSelector_) :
        audiumEngine(audiumEngine_),
        zoomHandler(zoomHandler_),
        colour(colour_),
        regionSelector(regionSelector_)
    {
        addKeyListener(this);
        setWantsKeyboardFocus(true);
    }

    virtual ~DraggerControl() override
    {
        removeKeyListener(this);
    }
    
    // Label metrics. paintLabel() draws the label at labelLeftInset and the
    // suffix immediately after it, separated by labelSuffixGap. Subclasses that
    // decide whether text fits have to measure against exactly these values, so
    // they live here rather than being restated at each call site.
    static constexpr float labelFontHeight = 12.0f;
    static constexpr float labelLeftInset  = 5.0f;
    static constexpr float labelSuffixGap  = 4.0f;

    // Centres the label in the draggerHeight strip: (19 - 12) / 2 = 3.5, rounded up.
    static constexpr int labelTopInset = 4;

    static juce::Font getLabelFont() { return juce::Font (juce::FontOptions (labelFontHeight)); }

    void paintLabel (juce::Graphics& g, const juce::String label, const juce::String suffix = {})
    {
        g.setFont (getLabelFont());

        auto labelWidth = GlyphArrangement::getStringWidth (g.getCurrentFont(), label);

        juce::Rectangle<int> bonds((int) labelLeftInset,
                                   labelTopInset,
                                   labelWidth,
                                   g.getCurrentFont().getHeight());

        g.setColour (getLabelColour());
        g.drawFittedText (label, bonds, juce::Justification::topLeft, 1);

        if (suffix.isNotEmpty())
        {
            juce::Rectangle<int> suffixBonds((int) (labelLeftInset + labelWidth + labelSuffixGap),
                                             labelTopInset,
                                             GlyphArrangement::getStringWidth (g.getCurrentFont(), suffix),
                                             g.getCurrentFont().getHeight());

            // A subtler alpha than the main label so the suffix reads as
            // secondary/auxiliary information.
            g.setColour (getLabelColour().withAlpha (0.75f));
            g.drawFittedText (suffix, suffixBonds, juce::Justification::topLeft, 1);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto colour = Colours::white;
        if (isSelected())
        {
            g.setColour (colour.withAlpha(0.8f));
        }
        else
        {
            g.setColour (colour.withAlpha(0.3f));
        }

        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component

        if (not isRecording())
            paintLabel(g, getLabelString(), getLabelSuffix());

    }

    enum Edge
    {
        leftEdge,
        rightEdge,
        middleEdge,
        outsideEdge
    };
        
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;
    
    bool keyPressed (const KeyPress& key, Component* originatingComponent) override;

    /// Cmd+Alt (Ctrl+Alt on Windows/Linux) turns an edge drag into a
    /// stretch. Plain Cmd, Shift and plain Alt are taken by selection and
    /// drag-and-drop, and Ctrl+click is the macOS right-click emulation -
    /// it must stay free for the clip context menu.
    static bool isStretchModifier (const juce::ModifierKeys& mods)
    {
        return mods.isCommandDown() && mods.isAltDown();
    }

    /// While the stretch overlay is open on this very clip, the clip is in
    /// stretch mode: plain edge drags stretch too, no modifiers needed.
    bool overlayStretchActive() const;

    void updateMouseZone (const juce::MouseEvent& e)
    {
        const auto stretching = isStretchModifier(e.mods) || overlayStretchActive();
        switch (getDragMode(e.getPosition().getX())) {
            case leftEdge:
                setMouseCursor (stretching ? juce::MouseCursor::LeftRightResizeCursor
                                           : juce::MouseCursor::LeftEdgeResizeCursor);
                break;
            case rightEdge:
                setMouseCursor (stretching ? juce::MouseCursor::LeftRightResizeCursor
                                           : juce::MouseCursor::RightEdgeResizeCursor);
                break;
            case middleEdge:
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                break;
            default:
                break;
        }
    }

    const Edge getDragMode(int x) const
    {
        auto w = getWidth();
        
        // edge case if width is very small
        if (w < 5)
            return middleEdge;
    
        auto border = std::min(getWidth() / 3, borderSize);
        
        if (x < border)
        {
            return leftEdge;
        }
        else if (getWidth() - x < border)
        {
            return rightEdge;
        }
        else
        {
            return middleEdge;
        }
    }
    
    void commitRangeToEngine(juce::Range<double> rangeInClocks)
    {
        // commit values to engine
        commitData(rangeInClocks, audium::clocks);
    }
    
    void commitData(const juce::Range<double> newData, audium::TimeContextType context);
    
    bool commitPositionData(const audium::PositionableBase &positionableBase,
                            const juce::Range<double> newRange,
                            const audium::TimeContextType context);
    
    virtual bool isSelected() const = 0;
    
    virtual void setSelected(bool bSelected, bool deselectOthers) = 0;
    
    virtual void shiftSelect() = 0;
    
    virtual const juce::String getLabelString() const = 0;

    /** @brief Secondary label text drawn after the main label at a reduced alpha. */
    virtual const juce::String getLabelSuffix() const { return {}; }

    virtual const juce::Colour getLabelColour() const = 0;
    
    virtual bool validateData() = 0;
    
    virtual bool isRecording() = 0;
    
    void setComponentToDrag(juce::Component* comp);
    
    void setPositionableObject(std::shared_ptr<audium::PositionableBase> object);
    
    static constexpr int draggerHeight = 19;
    
    juce::Point<float> mouseDownOffset;
    
    juce::Point<int> autoScrollOffset;
    
protected:
    
    juce::Component* componentToDrag = nullptr;
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;
    std::shared_ptr<audium::PositionableBase> positionableObject;
    
    Edge currentDragMode = outsideEdge;

    // True while an edge drag is a stretch (Ctrl+Alt): the source window
    // stays fixed and the drag sets the clip's speed ratio instead of
    // trimming.
    bool stretchDrag = false;
    
    const int borderSize = 10;
    const int minimumWidth = 2;

    juce::Rectangle<int> originalBounds;

    std::unique_ptr<audium::UndoableContainerAction> undoableContainerAction;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggerControl)
};
