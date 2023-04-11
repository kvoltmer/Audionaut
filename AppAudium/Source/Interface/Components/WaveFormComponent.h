/*
  ==============================================================================

    WaveFormComponent.h
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

#include "Interface/Handlers/ZoomHandler.h"

using namespace juce;


class AudioResource;


//==============================================================================
class WaveFormComponent  : public Component,
                           public ChangeListener,
                           public FileDragAndDropTarget,
                           public ChangeBroadcaster,
                           public ScrollBar::Listener,
                           private Timer
{
public:
    WaveFormComponent (std::shared_ptr<AudioResource> audioResource,
                       std::shared_ptr<ZoomHandler> zoomHandler);

    ~WaveFormComponent() override;
    
    void setAudioResource (std::shared_ptr<AudioResource> audioResource);
    
    URL getLastDroppedFile() const noexcept;

    void setFollowsTransport (bool shouldFollow);

    void paint (Graphics& g) override;

    void resized() override;

    void changeListenerCallback (ChangeBroadcaster*) override;

    bool isInterestedInFileDrag (const StringArray& /*files*/) override;

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override;

    void mouseDown (const MouseEvent& e) override;

    void mouseDrag (const MouseEvent& e) override;

    void mouseUp (const MouseEvent&) override;

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override;
    
private:
    
    std::shared_ptr<AudioResource> audioResource;

    std::shared_ptr<ZoomHandler> zoomHandler;
    
    std::unique_ptr<ResizableEdgeComponent> resizableEdgeComponent;
    std::unique_ptr<ResizableBorderComponent> resizableBorderComponent;
    
    bool isFollowingTransport = false;
    
    /// TODO: remove this
    URL lastFileDropped;

    DrawableRectangle currentPositionMarker;
    

    bool canMoveTransport() const noexcept
    {
        return ! (isFollowingTransport && audioResource->getAudioTransportSource()->isPlaying());
    }

    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    void timerCallback() override
    {
        if (canMoveTransport())
            updateCursorPosition();
        else
        {
            /// TODO: scrolling 
            jassertfalse;
            // setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
        }
    }

    void updateCursorPosition()
    {
        currentPositionMarker.setVisible (audioResource->getAudioTransportSource()->isPlaying() || isMouseButtonDown());

        currentPositionMarker.setRectangle (Rectangle<float> (zoomHandler->timeToX (audioResource->getAudioTransportSource()->getCurrentPosition()) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() /*  - scrollbar.getHeight()*/)));
        
    
    }
    
};
