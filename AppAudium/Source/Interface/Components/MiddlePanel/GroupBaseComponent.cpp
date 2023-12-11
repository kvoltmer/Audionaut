/*
  ==============================================================================

    GroupBaseComponent.cpp
    Created: 27 Nov 2023 12:23:48pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "GroupBaseComponent.h"


#include "Util/EngineAccess.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"
#include "Interface/ColourIds.h"
#include "Interface/Views/AudioRegionView.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudioGroupContainer.h"

using namespace audium;


GroupBaseComponent::GroupBaseComponent (std::shared_ptr<AudioGroup> group,
                                        std::shared_ptr<AudiumEngine> audiumEngine,
                                        std::shared_ptr<ZoomHandler> zoomHandler,
                                        std::shared_ptr<RegionSelector> regionSelector) :
    audioGroup(group),
    audiumEngine(audiumEngine),
    zoomHandler(zoomHandler),
    regionSelector(regionSelector)
{
    if (group->getColour() == juce::Colours::pink)
    {
        auto newColour = audium::getNewWaveFormColour();
        
        auto numGroups = audiumEngine->getAudioGroupContainer()->getNumItems();
        for (auto i = 0; i < numGroups; i++)
        {
            if(newColour == audiumEngine->getAudioGroupContainer()->getAudioGroup(i)->getColour())
            {
                newColour = audium::getNewWaveFormColour();
            }
        }
        
        group->setColour(newColour);
    }
}

void GroupBaseComponent::paint (juce::Graphics& g)
{
    if (externalDragAndDrop)
    {
        g.fillAll (findColour(audium::secondaryBackgroundColourId).brighter());
    }
}

void GroupBaseComponent::filesDropped (const StringArray& filenames, int x, int y)
{
    if ( !filenames.isEmpty())
    {
        auto transportPosition = zoomHandler->xToSeconds(x);
        auto channelPosition = 0;
        
        for (auto i = 0; i < filenames.size(); i++)
        {
            // check if we overlap with an existing resource (snap position to nearest resource)
            const auto resources = audioGroup->getAudioResources();
            for (auto resource : resources)
            {
                if (resource->containsAbsolutePosition(transportPosition))
                {
                    transportPosition = resource->getTransportPositionSeconds();
                    // position is below
                    channelPosition = audioGroup->getNumChannels();
                    break;
                }
            }
            
            auto url = URL (File (filenames[i]));
            auto audioResource = audiumEngine->getAudioResourceContainer()->addAudioResource(url, *audiumEngine, audioGroup, channelPosition, transportPosition);
            if (audioResource != nullptr)
            {
                audiumEngine->getAudioRegionContainer()->copyRegionsForResource(audioResource);
            }
        }
        
        // will update content
        refreshComponent(audioGroup, true);
    }
    
    externalDragAndDrop = false;
    regionSelector->setEnabled(true);
    repaint();
}

void GroupBaseComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    externalDragAndDrop = true;
    regionSelector->setEnabled(false);
    repaint();
}
void GroupBaseComponent::fileDragExit (const juce::StringArray& files)
{
    externalDragAndDrop = false;
    regionSelector->setEnabled(true);
    repaint();
}

void GroupBaseComponent::mouseDown (const MouseEvent& e)
{
    getParentComponent()->mouseDown(e);
    mouseDrag (e);
}

void GroupBaseComponent::mouseDrag (const MouseEvent& e)
{
    getParentComponent()->mouseDrag(e);
}

void GroupBaseComponent::mouseUp (const MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

void GroupBaseComponent::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    getParentComponent()->mouseWheelMove(e, wheel);
}
