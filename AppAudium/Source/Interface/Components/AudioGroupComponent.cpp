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
    
    rebuildComponents();
    
    resized();
    
}

void AudioGroupComponent::rebuildComponents()
{
    removeAllChildren();
    audioGroupRegions.clear();
    
    // get all play list items and create components
    auto playListContainer = audiumEngine->getPlayListContainer();
    for (auto i = 0; i < playListContainer->getNumItems(); i++)
    {
        auto playListItem = playListContainer->getPlayListItem(i);
        auto groupRegion = std::shared_ptr<AudioGroupRegionComponent>(new AudioGroupRegionComponent(audioResourceGroup, playListItem, zoomHandler));
        
        addAndMakeVisible(groupRegion.get());
        audioGroupRegions.push_back(groupRegion);
    }
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
    for (auto regionView : audioGroupRegions)
    {
        auto playListItem = regionView->getPlayListItem();
        auto start = zoomHandler->timeToXWithOffset(playListItem->getAbsolueStartTime());
        auto width = zoomHandler->timeToXWithOffset(playListItem->getDurationTime());
        regionView->setBounds(start, getLocalBounds().getY(), width, getLocalBounds().getHeight());
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



