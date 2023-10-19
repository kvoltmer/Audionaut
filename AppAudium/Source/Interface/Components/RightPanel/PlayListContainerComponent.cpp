/*
  ==============================================================================

    PlayListContainerComponent.cpp
    Created: 10 Oct 2023 10:33:05am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "PlayListContainerComponent.h"
#include "PlayListComponent.h"

//==============================================================================
PlayListContainerComponent::PlayListContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    createComponents();
}

PlayListContainerComponent::~PlayListContainerComponent()
{
}

void PlayListContainerComponent::updateUI()
{
    // TODO: don't recreate suff -> introduce a context
    createComponents();
    resized();
}

void PlayListContainerComponent::createComponents()
{
    removeAllChildren();
    playListComponents.clear();
    
    auto groups = audiumEngine->getAudioResourceContainer()->getAudioGroups();
    
    for (auto group : groups)
    {
        auto playListComponent = std::shared_ptr<PlayListComponent>(new PlayListComponent(audiumEngine, group));
        playListComponents.push_back(playListComponent);
        addAndMakeVisible(playListComponent.get());
    }
    
}

void PlayListContainerComponent::paint (juce::Graphics&)
{
}

void PlayListContainerComponent::resized()
{
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;

    for (auto playListComponent : playListComponents)
    {

        fb.items.add (juce::FlexItem (*playListComponent.get()).withFlex (0, 1, getWidth()));
    }

    fb.performLayout (getLocalBounds());
            

}
