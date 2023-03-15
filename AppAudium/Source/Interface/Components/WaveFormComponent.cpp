/*
  ==============================================================================

    WaveFormComponent.cpp
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "WaveFormComponent.h"
#include "Util/EngineAccess.h"

// iterating 2 palettes where the frist one has less colours to gain more variaty
static int currentWaveFormColour = 0;
static const int numWaveFormColours = 15;
static const uint32 waveFormColourScheme[numWaveFormColours] = {
    0xff70d6ff,0xffff70a6,0xffff9770,0xffffd670,0xffe9ff70, // first palette
    0xfffbf8cc,0xfffde4cf,0xffffcfd2,0xfff1c0e8,0xffcfbaf0,0xffa3c4f3,0xff90dbf4,0xff8eecf5,0xff98f5e1,0xffb9fbc0 // second palette
};


WaveFormComponent::WaveFormComponent (AudioTransportSource& source)
    : transportSource (source)
{
    
    audioResource = nullptr;

    addAndMakeVisible (scrollbar);
    scrollbar.setRangeLimits (visibleRange);
    scrollbar.setAutoHide (false);
    scrollbar.addListener (this);

    currentPositionMarker.setFill (Colours::white.withAlpha (0.85f));
    addAndMakeVisible (currentPositionMarker);
    
    currentColour = Colour(waveFormColourScheme[currentWaveFormColour++]);
    if (currentWaveFormColour >= numWaveFormColours) currentWaveFormColour = 0;
}

WaveFormComponent::~WaveFormComponent()
{
    scrollbar.removeListener (this);
 
    if (audioResource != nullptr)
    {
        audioResource->getThumbnail().removeChangeListener(this);
    }
}

void WaveFormComponent::setAudioResource (std::shared_ptr<AudioResource> audioResource)
{
    this->audioResource = audioResource;
    audioResource->getThumbnail().addChangeListener (this);

    Range<double> newRange (0.0, audioResource->getThumbnail().getTotalLength());
    scrollbar.setRangeLimits (newRange);
    setRange (newRange);

    startTimerHz (40);
}

void WaveFormComponent::setURL (const URL& url)
{
    auto resource = getAudiumEngine(this)->getAudioResourceContainer()->addAudioResource(url);
    if (resource != nullptr)
    {
        audioResource = resource;
        audioResource->getThumbnail().addChangeListener (this);

        Range<double> newRange (0.0, audioResource->getThumbnail().getTotalLength());
        scrollbar.setRangeLimits (newRange);
        setRange (newRange);

        startTimerHz (40);
    }
}

URL WaveFormComponent::getLastDroppedFile() const noexcept
{
    return lastFileDropped;
}

void WaveFormComponent::setZoomFactor (double amount)
{
    if (audioResource->getThumbnail().getTotalLength() > 0)
    {
        auto newScale = jmax (0.001, audioResource->getThumbnail().getTotalLength() * (1.0 - jlimit (0.0, 0.99, amount)));
        auto timeAtCentre = xToTime ((float) getWidth() / 2.0f);

        setRange ({ timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 });
    }
}

void WaveFormComponent::setRange (Range<double> newRange)
{
    visibleRange = newRange;
    scrollbar.setCurrentRange (visibleRange);
    updateCursorPosition();
    repaint();
}

void WaveFormComponent::setFollowsTransport (bool shouldFollow)
{
    isFollowingTransport = shouldFollow;
}

void WaveFormComponent::paint (Graphics& g)
{
    g.fillAll (currentColour.withAlpha(0.25f));
    g.setColour (currentColour);
    
//        auto a = getLocalBounds().getTopLeft();
//        auto b = getLocalBounds().getBottomRight();
//        ColourGradient gradient(Colours::lightgreen, a.getX(), a.getY(),
//                                Colours::lightyellow, b.getX(), b.getY(), false);
//        g.setGradientFill(gradient);

    if (audioResource != nullptr &&
        audioResource->getThumbnail().getTotalLength() > 0.0)
    {
        auto thumbArea = getLocalBounds();

        thumbArea.removeFromBottom (scrollbar.getHeight() + 4);
        audioResource->getThumbnail().drawChannels (g, thumbArea.reduced (2),
                                visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
    }
    else
    {
        g.setFont (14.0f);
        g.drawFittedText ("(Drag & drop audio files here)", getLocalBounds(), Justification::centred, 2);
    }
}

void WaveFormComponent::resized()
{
    scrollbar.setBounds (getLocalBounds().removeFromBottom (14).reduced (2));
}

void WaveFormComponent::changeListenerCallback (ChangeBroadcaster*)
{
    // this method is called by the thumbnail when it has changed, so we should repaint it..
    repaint();
}

bool WaveFormComponent::isInterestedInFileDrag (const StringArray& /*files*/)
{
    return true;
}

void WaveFormComponent::filesDropped (const StringArray& files, int /*x*/, int /*y*/)
{
    lastFileDropped = URL (File (files[0]));
    sendChangeMessage();
}

void WaveFormComponent::mouseDown (const MouseEvent& e)
{
    mouseDrag (e);
}

void WaveFormComponent::mouseDrag (const MouseEvent& e)
{
    if (canMoveTransport())
        transportSource.setPosition (jmax (0.0, xToTime ((float) e.x)));
}

void WaveFormComponent::mouseUp (const MouseEvent&)
{
    transportSource.start();
}

void WaveFormComponent::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel)
{
    /* TODO
    if (getThumbnail().getTotalLength() > 0.0)
    {
        auto newStart = visibleRange.getStart() - wheel.deltaX * (visibleRange.getLength()) / 10.0;
        newStart = jlimit (0.0, jmax (0.0, getThumbnail().getTotalLength() - (visibleRange.getLength())), newStart);

        if (canMoveTransport())
            setRange ({ newStart, newStart + visibleRange.getLength() });

        if (wheel.deltaY != 0.0f)
            zoomSlider.setValue (zoomSlider.getValue() - wheel.deltaY);

        repaint();
    }
    */
}
