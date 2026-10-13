//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Controls/RectangleMarker.h"
#include "Interface/Handlers/ZoomHandler.h"

/**
 * Overlay that tracks either a single transport position (a thin vertical
 * line) or a time range (a filled block), polled from the engine on every
 * display refresh via VBlankAttachment. Painting is done by RectangleMarker,
 * so each refresh only invalidates the strip the marker left and the strip it
 * moved to. Sub-pixel positions are painted as-is: with the update locked to
 * the display's vertical blank the line advances evenly instead of stepping.
 */
class PositionMarker  : public RectangleMarker
{
public:
    PositionMarker (std::shared_ptr<ZoomHandler> zoomHandler_) :
        zoomHandler (zoomHandler_)
    {
        setColour (juce::Colours::pink);
    }

    ~PositionMarker() override = default;

    void setColour (juce::Colour colour)
    {
        setFill (colour);
    }

    void updateCursorPosition()
    {
        auto height = static_cast<float> (getHeight() - zoomHandler->getScrollBarHeight());

        if (onUpdatePosition != nullptr)
        {
            auto xPos = static_cast<float> (zoomHandler->secondsToXWithOffset (onUpdatePosition (audium::seconds)));

            if (xPos >= 0.0f)
                setRectangle ({ xPos - 0.75f, 0.0f, 1.5f, height });
            else
                setRectangle ({});
        }
        else if (onUpdateRange != nullptr)
        {
            auto range = onUpdateRange (audium::seconds);
            auto x = static_cast<float> (zoomHandler->secondsToXWithOffset (range.getStart()));
            auto w = static_cast<float> (zoomHandler->secondsToX (range.getLength()));
            setRectangle ({ x, 0.0f, w, height });
        }
    }

    std::function<double (const audium::TimeContextType context)> onUpdatePosition = nullptr;

    std::function<juce::Range<double> (const audium::TimeContextType context)> onUpdateRange = nullptr;

private:
    std::shared_ptr<ZoomHandler> zoomHandler;

    // Declared last so it is torn down before anything the callback touches.
    juce::VBlankAttachment vBlankAttachment { this, [this]
    {
        if (auto* parent = getParentComponent(); parent != nullptr && parent->isVisible())
            updateCursorPosition();
    } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PositionMarker)
};
