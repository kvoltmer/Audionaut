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


using namespace audium;


AudioGroupComponent::AudioGroupComponent (std::shared_ptr<AudioResourceGroup> group,
                                          std::shared_ptr<AudiumEngine> audiumEngine,
                                          std::shared_ptr<ZoomHandler> zoomHandler) :
    audiumEngine(audiumEngine),
    zoomHandler(zoomHandler)
{
    group->setColour(audium::getNewWaveFormColour());
    setAudioResourceGroup(group);
}

AudioGroupComponent::~AudioGroupComponent()
{
}

void AudioGroupComponent::setAudioResourceGroup (std::shared_ptr<AudioResourceGroup> group)
{
    audioResourceGroup = group;
    audioResourceGroup->updateColour();
    
    zoomHandler->updateTotalLength();
    
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
    
    auto playListContainer = audiumEngine->getPlayListContainer();
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

void AudioGroupComponent::paint (juce::Graphics& g)
{
    if (externalDragAndDrop)
    {
        g.fillAll (findColour(audium::secondaryBackgroundColourId).brighter());
    }
}


void AudioGroupComponent::resized()
{
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

void AudioGroupComponent::filesDropped (const StringArray& filenames, int /*x*/, int /*y*/)
{
    if ( !filenames.isEmpty())
    {
        for (auto i = 0; i < filenames.size(); i++)
        {
            auto url = URL (File (filenames[i]));
            audiumEngine->getAudioResourceContainer()->addAudioResource(url, audioResourceGroup);
        }
        audiumEngine->createDefaultRegionAndPlayList();
        
        // will update content
        setAudioResourceGroup(audioResourceGroup);
    }
    
    externalDragAndDrop = false;
    repaint();
}

void AudioGroupComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    externalDragAndDrop = true;
    repaint();
}
void AudioGroupComponent::fileDragExit (const juce::StringArray& files)
{
    externalDragAndDrop = false;
    repaint();
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



