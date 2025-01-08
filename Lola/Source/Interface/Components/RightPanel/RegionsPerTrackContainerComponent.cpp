/*
  ==============================================================================

    RegionsPerTrackContainerComponent.cpp
    Created: 14 Dec 2024 11:10:49am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "RegionsPerTrackContainerComponent.h"
#include "RegionsPerTrackComponent.h"

#include "Engine/AudiumEngine.h"
#include "Interface/ColourIds.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Group/AudioTrackContainer.h"

//==============================================================================
RegionsPerTrackContainerComponent::RegionsPerTrackContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    
    updateUI();
}

void RegionsPerTrackContainerComponent::updateUI(UIContext context)
{
    if (context == RebuildContext)
    {
        createComponents();
        resized();
    }
    
    for (auto component : regionsPerTrackComponents)
    {
        if (context == SelectionContext)
        {
            component->updateSelection();
        }
        else
        {
            component->updateUI();
        }
    }

}

void RegionsPerTrackContainerComponent::createComponents()
{
    removeAllChildren();
    regionsPerTrackComponents.clear();
    
    auto tracks = audiumEngine->getAudioTrackContainer()->getAudioTracks();
    
    for (auto track : tracks) {
        auto component = std::shared_ptr<RegionsPerTrackComponent>(new RegionsPerTrackComponent(audiumEngine, track));
        regionsPerTrackComponents.push_back(component);
        addAndMakeVisible(component.get());
    }
}


void RegionsPerTrackContainerComponent::resized()
{
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;

    for (auto component : regionsPerTrackComponents)
    {
        fb.items.add (juce::FlexItem (*component.get()).withFlex (0, 1, getWidth()));
    }
    
    fb.performLayout (getLocalBounds());
}
