/*
  ==============================================================================

    PlayListItemComponent.cpp
    Created: 28 Sep 2023 12:07:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "PlayListItemComponent.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/AudioResourceContainer.h"
#include "Interface/Views/AudioRegionView.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Interface/ColourIds.h"

//==============================================================================
PlayListItemComponent::PlayListItemComponent(std::shared_ptr<AudiumEngine> audiumEngine,
                                             std::shared_ptr<AudioGroup> audioGroup,
                                             std::shared_ptr<PlayListItem> playListItem,
                                             std::shared_ptr<ZoomHandler> zoomHandler,
                                             std::shared_ptr<RegionSelector> regionSelector) :
    audiumEngine(audiumEngine),
    audioGroup(audioGroup),
    playListItem(playListItem),
    regionSelector(regionSelector)
{
    // this component doesn't handle mouse events
    setInterceptsMouseClicks(false, false);

    // create views
    auto audioResources = audioGroup->getAudioResources();
    auto rowNumber = 0;
    for (auto audioResource : audioResources)
    {
        auto view = std::shared_ptr<AudioRegionView>(new AudioRegionView(audiumEngine,
                                                                         audioResource,
                                                                         zoomHandler,
                                                                         playListItem->getRegion(),
                                                                         audioGroup->getColour(),
                                                                         regionSelector,
                                                                         rowNumber));
        addAndMakeVisible(view.get());
        children.push_back(view);
    }
}

PlayListItemComponent::~PlayListItemComponent()
{
}

void PlayListItemComponent::paint (juce::Graphics& g)
{
    if (playListItem->isSelected())
    {
        g.setColour (audium::getComplementaryColour(audioGroup->getColour()).darker());
        g.fillAll();
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha(0.50f));
        g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 2.0f);
    }
    
}

void PlayListItemComponent::resized()
{
    int top = 0;
    int count = 0;
    auto audioResources = audioGroup->getAudioResources();
    for (auto audioResource : audioResources)
    {
        auto height = 100;//audioResource->getHeight();
        if (count < children.size())
        {
            auto child = children[count];
            if (child != nullptr)
            {
                child->setBounds(0, top, getWidth(), height);
            }
            count++;
        }
        top += height;
    }
}
