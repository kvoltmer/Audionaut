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
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioGroup.h"


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
    setNewGroupColour();
    setInterceptsMouseClicks(false, true);
}

void GroupBaseComponent::setNewGroupColour()
{
    if (audioGroup->getColour() == juce::Colours::pink)
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
        
        audioGroup->setColour(newColour);
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
            std::shared_ptr<AudioSubGroup> subGroup = nullptr;
            for (auto resource : resources)
            {
                if (resource->containsAbsolutePosition(transportPosition, audium::seconds))
                {
                    transportPosition = resource->getTransportPosition(audium::seconds);
                    // position is below
                    channelPosition = audioGroup->getNumChannels();
                    subGroup = resource->getAudioSubGroup();
                    break;
                }
            }
            if (subGroup == nullptr)
            {
                subGroup = audioGroup->createNewAudioSubGroup(*audiumEngine->getAudioResourceContainer(),
                                                              *audiumEngine->getAudioRegionContainer());
            }
            
            auto url = URL (File (filenames[i]));
            auto audioResource = audiumEngine->getAudioResourceContainer()->addAudioResource(url,
                                                                                             *audiumEngine,
                                                                                             audioGroup,
                                                                                             subGroup,
                                                                                             channelPosition,
                                                                                             transportPosition);
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
