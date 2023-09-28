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
#include "Interface/Controls/AudioGroupListBox.h"
#include "Interface/ColourIds.h"
#include "Interface/Views/AudioRegionView.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"

// iterating 2 palettes where the frist one has less colours to gain more variaty
static int currentWaveFormColour = 0;
static const int numWaveFormColours = 15;
static const uint32 waveFormColourScheme[numWaveFormColours] = {
    0xff70d6ff,0xffff70a6,0xffff9770,0xffffd670,0xffe9ff70, // first palette
    0xfffbf8cc,0xfffde4cf,0xffffcfd2,0xfff1c0e8,0xffcfbaf0,0xffa3c4f3,0xff90dbf4,0xff8eecf5,0xff98f5e1,0xffb9fbc0 // second palette
};


WaveFormComponent::WaveFormComponent (std::shared_ptr<AudioResource> audioResource,
                                      std::shared_ptr<PlayListContainer> playListContainer,
                                      std::shared_ptr<ZoomHandler> zoomHandler) :
    playListContainer(playListContainer),
    zoomHandler(zoomHandler)
{
    jassert(this->playListContainer);
    jassert(this->zoomHandler);
    
    setAudioResource(audioResource);
    
    jassert(this->audioResource);
    
    /// simply iteraterate our colour scheme and assign our current colour
    assert(this->audioResource);
    this->audioResource->currentColour = Colour(waveFormColourScheme[currentWaveFormColour++]);
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

void WaveFormComponent::setAudioResource (std::shared_ptr<AudioResource> resource)
{
    //std::cout << "WaveFormComponent::setAudioResource" << std::endl;
    zoomHandler->updateTotalLength();
    if (audioResource != nullptr)
    {
        audioResource->getThumbnail().removeChangeListener(this);
    }
    
    audioResource = resource;
    audioResource->getThumbnail().addChangeListener (this);
    
    
    /// TODO: improve this
    removeAllChildren();
    audioRegionViews.clear();
//
//    for (auto i = 0; i < playListContainer->getNumItems(); i++)
//    {
//        auto playListItem = playListContainer->getPlayListItem(i);
//        auto region = playListItem->getRegion();
//        std::shared_ptr<AudioRegionView> regionView = std::shared_ptr<AudioRegionView>(new AudioRegionView(audioResource, zoomHandler, region));
//        addAndMakeVisible(regionView.get());
//        audioRegionViews.push_back(regionView);
//    }
    
    jassert(playListContainer->getPlayListItem(0));
    auto region = playListContainer->getPlayListItem(0)->getRegion();
    std::shared_ptr<AudioRegionView> regionView = std::shared_ptr<AudioRegionView>(new AudioRegionView(audioResource, zoomHandler, region));
    addAndMakeVisible(regionView.get());
    audioRegionViews.push_back(regionView);
    
    resized();
    
}


void WaveFormComponent::scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
}


void WaveFormComponent::resized()
{
    if (audioResource != nullptr &&
        audioResource->getHeight() != getBounds().getHeight())
    {
        /// TODO: change height
        // audioResource->getHeight() = getBounds().getHeight();
        
        if (AudioGroupListBox* list = this->findParentComponentOfClass<AudioGroupListBox>())
        {
            list->updateContent();
        }
        
    }
    
    for (auto regionView: audioRegionViews)
    {
//        auto region = regionView->getAudioRegion();
//        auto start = zoomHandler->timeToXWithOffset(region->position.getStart());
//        auto end = zoomHandler->timeToXWithOffset(region->position.getEnd());
//        auto width = end - start;
//        regionView->setSize (width, getHeight());
        regionView->setSize (getWidth(), getHeight());
    }
    
}

void WaveFormComponent::changeListenerCallback (ChangeBroadcaster*)
{
    // this method is called by the thumbnail when it has changed, so we should repaint it..
    repaint();
    
    for (auto views: audioRegionViews)
    {
        views->repaint();
    }
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



