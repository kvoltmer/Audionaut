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
#include "Interface/Controls/WaveFormTableListBox.h"
#include "Interface/ColourIds.h"

// iterating 2 palettes where the frist one has less colours to gain more variaty
static int currentWaveFormColour = 0;
static const int numWaveFormColours = 15;
static const uint32 waveFormColourScheme[numWaveFormColours] = {
    0xff70d6ff,0xffff70a6,0xffff9770,0xffffd670,0xffe9ff70, // first palette
    0xfffbf8cc,0xfffde4cf,0xffffcfd2,0xfff1c0e8,0xffcfbaf0,0xffa3c4f3,0xff90dbf4,0xff8eecf5,0xff98f5e1,0xffb9fbc0 // second palette
};


WaveFormComponent::WaveFormComponent (std::shared_ptr<AudioResource> audioResource,
                                      std::shared_ptr<ZoomHandler> zoomHandler) :
    zoomHandler(zoomHandler)
{

    setAudioResource(audioResource);
    
    /// simply iteraterate our colour scheme and assign our current colour
    assert(this->audioResource);
    this->audioResource->currentColour = Colour(waveFormColourScheme[currentWaveFormColour++]);
    if (currentWaveFormColour >= numWaveFormColours)
        currentWaveFormColour = 0;
    
    resizableEdgeComponent.reset(new ResizableEdgeComponent(this, nullptr, ResizableEdgeComponent::bottomEdge));
    //addAndMakeVisible(resizableEdgeComponent.get());
    auto rect = getBounds();
    rect.setHeight(0);
    resizableEdgeComponent->setBounds(rect);
    
//    resizableBorderComponent.reset(new ResizableBorderComponent(this, nullptr));
//    addAndMakeVisible(resizableBorderComponent.get());
//    resizableBorderComponent->setBounds(getBounds());
    
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
    zoomHandler->updateTotalLength();
    this->audioResource = audioResource;
    audioResource->getThumbnail().addChangeListener (this);
}

void WaveFormComponent::scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
//    if (! (isFollowingTransport && audioResource->getAudioTransportSource()->isPlaying()))
//    {
//    }
}

void WaveFormComponent::paint (Graphics& g)
{
    if (audioResource != nullptr &&
        audioResource->getThumbnail().getTotalLength() > 0.0)
    {
        g.fillAll (audioResource->currentColour.withAlpha(0.25f));
        g.setColour (audioResource->currentColour);
        
        auto thumbArea = getLocalBounds();
    
#if 1 /// draw visible range
        
        // the visible range is the scrollbar's range
        auto visibleRange = zoomHandler->getVisibleRange();
        
        // adjust the drawing area
        thumbArea.setX(static_cast<int>(visibleRange.getStart()));
        thumbArea.setWidth(static_cast<int>(visibleRange.getLength()));
                       
        
        auto rangeInSeconds = zoomHandler->getVisibleRangeInSeconds();
        
        audioResource->getThumbnail().drawChannels (g, thumbArea,
                                                    rangeInSeconds.getStart(), rangeInSeconds.getEnd(), 1.0f);
        
//        std::cout << "DRAW visible start " << visibleRange.getStart() << " length " << visibleRange.getLength() << std::endl;
//        std::cout << "DRAW seconds start " << rangeInSeconds.getStart() << " length " << rangeInSeconds.getLength() << std::endl;

#else /// draw entire waveform at once
        
        auto totalRange = zoomHandler->getTotalRange();
        audioResource->getThumbnail().drawChannels (g, thumbArea,
                                                    totalRange.getStart(), totalRange.getEnd(), 1.0f);
#endif
        
        /// draw filename label
        /// offset is x = 5, y = 5
        /// background is expanded by 2 pixels
        
        g.setFont (12.0f);
        
        Rectangle<int> bonds(zoomHandler->getVisibleRange().getStart() + 5,
                             5,
                             g.getCurrentFont().getStringWidth(audioResource->getFileNameWithoutExtension()),
                             g.getCurrentFont().getHeight());
        
        g.setColour(Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle (bonds.expanded(2, 2).toFloat(), 3.0f);
        
        g.setColour (findColour(audium::defaultTextColourId));
        g.drawFittedText (audioResource->getFileNameWithoutExtension(), bonds, Justification::topLeft, 1);
    }
    else
    {
        g.setFont (14.0f);
        g.drawFittedText ("audio resource not available", getLocalBounds(), Justification::centred, 2);
    }
    
    
//    auto thumbArea = getLocalBounds();
//    g.setColour (Colours::yellow);
//    g.fillRoundedRectangle (thumbArea.toFloat(), 3.0f);


    
}

void WaveFormComponent::resized()
{
    if (resizableBorderComponent) resizableBorderComponent->setBounds(getBounds());
    
    if (resizableEdgeComponent) resizableEdgeComponent->setBounds(getBounds());
    
    if (audioResource != nullptr &&
        audioResource->height != getBounds().getHeight())
    {
        audioResource->height = getBounds().getHeight();
        
        if (WaveFormTableListBox* list = this->findParentComponentOfClass<WaveFormTableListBox>())
        {
            list->updateContent();
        }
        
    }
    
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
    /// TODO: replace current audio resource
    sendChangeMessage();
}

void WaveFormComponent::mouseDown (const MouseEvent& e)
{
    getParentComponent()->mouseDown(e);
    mouseDrag (e);
}

void WaveFormComponent::mouseDrag (const MouseEvent& e)
{
    getParentComponent()->mouseDrag(e);
    
//    if (canMoveTransport())
//        audioResource->getAudioTransportSource()->setPosition (jmax (0.0, zoomHandler->xToTime ((float) e.x)));
//
}

void WaveFormComponent::mouseUp (const MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

void WaveFormComponent::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    getParentComponent()->mouseWheelMove(e, wheel);
}



