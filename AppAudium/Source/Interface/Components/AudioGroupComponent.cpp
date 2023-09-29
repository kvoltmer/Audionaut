/*
  ==============================================================================

    AudioGroupComponent.cpp
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioGroupComponent.h"
#include "Util/EngineAccess.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Interface/Components/AudioGroupRegionComponent.h"
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


AudioGroupComponent::AudioGroupComponent (std::shared_ptr<AudioResourceGroup> audioResourceGroup,
                                      std::shared_ptr<PlayListContainer> playListContainer,
                                      std::shared_ptr<ZoomHandler> zoomHandler) :
    playListContainer(playListContainer),
    zoomHandler(zoomHandler)
{
    jassert(this->playListContainer);
    jassert(this->zoomHandler);
    
    setAudioResourceGroup(audioResourceGroup);
    
    jassert(this->audioResourceGroup);
    
    /// simply iteraterate our colour scheme and assign our current colour
    assert(this->audioResourceGroup);
    this->audioResourceGroup->setColour(Colour(waveFormColourScheme[currentWaveFormColour++]));
    if (currentWaveFormColour >= numWaveFormColours)
        currentWaveFormColour = 0;
    
}

AudioGroupComponent::~AudioGroupComponent()
{
}

void AudioGroupComponent::setAudioResourceGroup (std::shared_ptr<AudioResourceGroup> resourceGroup)
{
    //std::cout << "AudioGroupComponent::setAudioResource" << std::endl;
    zoomHandler->updateTotalLength();
    
    audioResourceGroup = resourceGroup;
    
    /// TODO: improve this
    removeAllChildren();
    audioGroupRegions.clear();
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
    //std::shared_ptr<AudioRegionView> regionView = std::shared_ptr<AudioRegionView>(new AudioRegionView(audioResourceGroup, zoomHandler, region));
    
    std::shared_ptr<AudioGroupRegionComponent> groupRegion = std::shared_ptr<AudioGroupRegionComponent>(new AudioGroupRegionComponent(audioResourceGroup, region, zoomHandler));
    
    addAndMakeVisible(groupRegion.get());
    audioGroupRegions.push_back(groupRegion);
   
    // don't forget to update the region views
    for (auto view : audioGroupRegions)
    {
        //setAudioResourceGroup(audioResourceGroup);
    }
    
    resized();
    
}


void AudioGroupComponent::scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
}


void AudioGroupComponent::resized()
{
    /// TODO: implement change of height
//    if (audioResourceGroup != nullptr &&
//        audioResourceGroup->getHeight() != getBounds().getHeight())
//    {
//        /// TODO: change height
//        // audioResource->getHeight() = getBounds().getHeight();
//
//        if (AudioGroupListBox* list = this->findParentComponentOfClass<AudioGroupListBox>())
//        {
//            list->updateContent();
//        }
//
//    }
    
    for (auto regionView: audioGroupRegions)
    {
//        auto region = regionView->getAudioRegion();
//        auto start = zoomHandler->timeToXWithOffset(region->position.getStart());
//        auto end = zoomHandler->timeToXWithOffset(region->position.getEnd());
//        auto width = end - start;
//        regionView->setSize (width, getHeight());
        // regionView->setSize (getWidth(), getHeight());
        regionView->setBounds(getLocalBounds());
    }
    
}

bool AudioGroupComponent::isInterestedInFileDrag (const StringArray& /*files*/)
{
    return true;
}

void AudioGroupComponent::filesDropped (const StringArray& files, int /*x*/, int /*y*/)
{
    /// TODO: replace current audio resource
    sendChangeMessage();
}

void AudioGroupComponent::mouseDown (const MouseEvent& e)
{
    getParentComponent()->mouseDown(e);
    mouseDrag (e);
}

void AudioGroupComponent::mouseDrag (const MouseEvent& e)
{
    getParentComponent()->mouseDrag(e);
}

void AudioGroupComponent::mouseUp (const MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

void AudioGroupComponent::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    getParentComponent()->mouseWheelMove(e, wheel);
}



