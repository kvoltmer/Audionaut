/*
  ==============================================================================

    ArrangementGroupComponent.cpp
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "ArrangementGroupComponent.h"
#include "Util/EngineAccess.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"
#include "Interface/ColourIds.h"
#include "Interface/Views/AudioRegionView.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"


using namespace audium;


ArrangementGroupComponent::ArrangementGroupComponent (std::shared_ptr<AudioGroup> group,
                                          std::shared_ptr<AudiumEngine> audiumEngine,
                                          std::shared_ptr<ZoomHandler> zoomHandler) :
    audiumEngine(audiumEngine),
    zoomHandler(zoomHandler)
{
    if (group->getColour() == juce::Colours::pink)
    {
        group->setColour(audium::getNewWaveFormColour());
    }
    refreshComponent(group);
}

ArrangementGroupComponent::~ArrangementGroupComponent()
{
}

void ArrangementGroupComponent::refreshComponent (std::shared_ptr<AudioGroup> group, bool forceRebuildComponents)
{
    audioGroup = group;
    
    if (mustRebuildComponents() ||
        forceRebuildComponents)
    {
        rebuildComponents();
    }
    resized();
}

bool ArrangementGroupComponent::mustRebuildComponents() const
{
    // compare play list items
    auto playListContainer = audiumEngine->getPlayListContainer(audioGroup);
    auto playListItems = playListContainer->getPlayListItems();
    
    if (playListItems.size() != playListItemComponents.size())
    {
        return true;
    }
    
    for (auto i = 0; i < playListItems.size(); i++)
    {
        if (playListItems[i] != playListItemComponents[i]->getPlayListItem())
        {
            return true;
        }
    }
    
    return false;
}

void ArrangementGroupComponent::rebuildComponents()
{
    std::cout << "ArrangementGroupComponent::rebuildComponents " << audioGroup->getId() << std::endl;
    removeAllChildren();
    playListItemComponents.clear();
    
    // get all play list items and create components
    auto playListContainer = audiumEngine->getPlayListContainer(audioGroup);
    jassert(playListContainer);
    auto playListItems = playListContainer->getPlayListItems();
    
    for (auto playListItem : playListItems)
    {
        auto groupRegion = std::shared_ptr<PlayListItemComponent>(new PlayListItemComponent(audioGroup, playListItem, zoomHandler));
        
        addAndMakeVisible(groupRegion.get());
        playListItemComponents.push_back(groupRegion);
    }
}

void ArrangementGroupComponent::paint (juce::Graphics& g)
{
    if (externalDragAndDrop)
    {
        g.fillAll (findColour(audium::secondaryBackgroundColourId).brighter());
    }
}


void ArrangementGroupComponent::resized()
{
    for (auto regionView : playListItemComponents)
    {
        auto playListItem = regionView->getPlayListItem();
        auto start = zoomHandler->clocksToX(playListItem->getAbsolueStartTime());
        auto width = zoomHandler->clocksToX(playListItem->getDurationTimeInClocks());
        regionView->setBounds(start, getLocalBounds().getY(), width, getLocalBounds().getHeight());
    }
}

void ArrangementGroupComponent::filesDropped (const StringArray& filenames, int /*x*/, int /*y*/)
{
    if ( !filenames.isEmpty())
    {
        for (auto i = 0; i < filenames.size(); i++)
        {
            auto url = URL (File (filenames[i]));
            audiumEngine->getAudioResourceContainer()->addAudioResource(url, *audiumEngine, audioGroup);
        }
        audiumEngine->createDefaultRegionAndPlayList(audioGroup);
        
        // will update content
        refreshComponent(audioGroup, true);
    }
    
    externalDragAndDrop = false;
    repaint();
}

void ArrangementGroupComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    externalDragAndDrop = true;
    repaint();
}
void ArrangementGroupComponent::fileDragExit (const juce::StringArray& files)
{
    externalDragAndDrop = false;
    repaint();
}

void ArrangementGroupComponent::mouseDown (const MouseEvent& e)
{
    getParentComponent()->mouseDown(e);
    mouseDrag (e);
}

void ArrangementGroupComponent::mouseDrag (const MouseEvent& e)
{
    getParentComponent()->mouseDrag(e);
}

void ArrangementGroupComponent::mouseUp (const MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

void ArrangementGroupComponent::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    getParentComponent()->mouseWheelMove(e, wheel);
}



