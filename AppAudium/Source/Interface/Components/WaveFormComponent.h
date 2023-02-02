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
                           private ScrollBar::Listener,
                           private Timer
{
public:
    WaveFormComponent (AudioTransportSource& source);

    ~WaveFormComponent() override;

    void setURL (const URL& url);

    URL getLastDroppedFile() const noexcept;

    void setZoomFactor (double amount);

    void setRange (Range<double> newRange);

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

    ScrollBar scrollbar  { false };

    std::shared_ptr<AudioResource> audioResource;

    Range<double> visibleRange;
    bool isFollowingTransport = false;
    URL lastFileDropped;

    DrawableRectangle currentPositionMarker;

    float timeToX (const double time) const
    {
        if (visibleRange.getLength() <= 0)
            return 0;

        return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
    }

    double xToTime (const float x) const
    {
        return (x / (float) getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }

    bool canMoveTransport() const noexcept
    {
        return ! (isFollowingTransport && transportSource.isPlaying());
    }

    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &scrollbar)
            if (! (isFollowingTransport && transportSource.isPlaying()))
                setRange (visibleRange.movedToStartAt (newRangeStart));
    }

    void timerCallback() override
    {
        if (canMoveTransport())
            updateCursorPosition();
        else
            setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
    }

    void updateCursorPosition()
    {
        currentPositionMarker.setVisible (transportSource.isPlaying() || isMouseButtonDown());

        currentPositionMarker.setRectangle (Rectangle<float> (timeToX (transportSource.getCurrentPosition()) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() - scrollbar.getHeight())));
    }
};
