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
    WaveFormComponent (AudioTransportSource& source, ScrollBar& scrollbar);

    ~WaveFormComponent() override;
    
    void setAudioResource (std::shared_ptr<AudioResource> audioResource);

    URL getLastDroppedFile() const noexcept;

    void setTotalRangeInSeconds (Range<double> newRange);

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
    AudioTransportSource& transportSource;

    ScrollBar& scrollbar;

    std::shared_ptr<AudioResource> audioResource;

    Range<double> totalRange;
    bool isFollowingTransport = false;
    URL lastFileDropped;

    DrawableRectangle currentPositionMarker;
    
    Colour currentColour;

    float timeToX (const double time) const
    {
        if (totalRange.getLength() <= 0)
            return 0;

        return (float) getWidth() * (float) ((time - totalRange.getStart()) / totalRange.getLength());
    }

    double xToTime (const float x) const
    {
        return (x / (float) getWidth()) * (totalRange.getLength()) + totalRange.getStart();
    }

    bool canMoveTransport() const noexcept
    {
        return ! (isFollowingTransport && transportSource.isPlaying());
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
        currentPositionMarker.setVisible (transportSource.isPlaying() || isMouseButtonDown());

        currentPositionMarker.setRectangle (Rectangle<float> (timeToX (transportSource.getCurrentPosition()) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() /*  - scrollbar.getHeight()*/)));
    }
};
