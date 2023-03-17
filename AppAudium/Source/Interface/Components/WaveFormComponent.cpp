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


WaveFormComponent::WaveFormComponent (AudioTransportSource& source, std::shared_ptr<ZoomHandler> zoomHandler) :
    transportSource (source),
    zoomHandler(zoomHandler)
{
    audioResource = nullptr;
    

    currentPositionMarker.setFill (Colours::white.withAlpha (0.85f));
    addAndMakeVisible (currentPositionMarker);
    
    /// simply iteraterate our colour scheme and assign our current colour
    currentColour = Colour(waveFormColourScheme[currentWaveFormColour++]);
    if (currentWaveFormColour >= numWaveFormColours)
        currentWaveFormColour = 0;
}

WaveFormComponent::~WaveFormComponent()
{
    if (audioResource != nullptr)
    {
        audioResource->getThumbnail().removeChangeListener(this);
    }
}

void WaveFormComponent::setAudioResource (std::shared_ptr<AudioResource> audioResource)
{
    this->audioResource = audioResource;
    audioResource->getThumbnail().addChangeListener (this);

    startTimerHz (40);
}

URL WaveFormComponent::getLastDroppedFile() const noexcept
{
    return lastFileDropped;
}

void WaveFormComponent::scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (! (isFollowingTransport && transportSource.isPlaying()))
    {
    }
}

void WaveFormComponent::setFollowsTransport (bool shouldFollow)
{
    isFollowingTransport = shouldFollow;
}

void WaveFormComponent::paint (Graphics& g)
{
    g.fillAll (currentColour.withAlpha(0.25f));
    g.setColour (currentColour);
    

    if (audioResource != nullptr &&
        audioResource->getThumbnail().getTotalLength() > 0.0)
    {
        auto thumbArea = getLocalBounds();
    
#if 1 /// visible range only
        
        // the visible range is the scrollbar's range
        auto visibleRange = zoomHandler->getCurrentRange();
        
        // adjust the drawing area
        thumbArea.setX(visibleRange.getStart());
        thumbArea.setWidth(visibleRange.getLength());
                       
        // convert pixels to seconds (drawChannels expects start and end in seconds)
        Range<double> rangeInSeconds(zoomHandler->xToTime(visibleRange.getStart()), zoomHandler->xToTime(visibleRange.getEnd()));
        
        audioResource->getThumbnail().drawChannels (g, thumbArea,
                                                    rangeInSeconds.getStart(), rangeInSeconds.getEnd(), 1.0f);
        
//        std::cout << "DRAW visible start " << visibleRange.getStart() << " length " << visibleRange.getLength() << std::endl;
//        std::cout << "DRAW seconds start " << rangeInSeconds.getStart() << " length " << rangeInSeconds.getLength() << std::endl;

#else /// draw entire waveform at once
        
        audioResource->getThumbnail().drawChannels (g, thumbArea,
                                                    totalRange.getStart(), totalRange.getEnd(), 1.0f);
#endif
    }
    else
    {
        g.setFont (14.0f);
        g.drawFittedText ("(Drag & drop audio files here)", getLocalBounds(), Justification::centred, 2);
    }
}

void WaveFormComponent::resized()
{
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
        transportSource.setPosition (jmax (0.0, zoomHandler->xToTime ((float) e.x)));
}

void WaveFormComponent::mouseUp (const MouseEvent&)
{
    transportSource.start();
}

void WaveFormComponent::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel)
{
}



