//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "PlayListContainerComponent.h"
#include "PlayListComponent.h"
#include "Interface/ColourIds.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

//==============================================================================
PlayListContainerComponent::PlayListContainerComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    createComponents();
}

PlayListContainerComponent::~PlayListContainerComponent()
{
}

void PlayListContainerComponent::updateUI(UIContext context)
{
    auto tracks = audiumEngine->getAudioTrackContainer()->getAudioTracks();
    
    if (context == RebuildContext ||
        tracks.size() != playListComponents.size()) {
        createComponents();
        resized();
    }
    else {
        auto i = 0;
        for (auto track : tracks) {
            
            if (i < playListComponents.size()) {
                
                if (context == SelectionContext) {
                    playListComponents[i]->updateSelection();
                }
                else {
                    playListComponents[i]->updateUI(track);
                }
            }
            i++;
        }
    }
}

void PlayListContainerComponent::createComponents()
{
    removeAllChildren();
    playListComponents.clear();
    
    auto tracks = audiumEngine->getAudioTrackContainer()->getAudioTracks();
    
    for (auto track : tracks) {
        auto playListComponent = std::shared_ptr<PlayListComponent>(new PlayListComponent(audiumEngine, track));
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
    
    for (auto playListComponent : playListComponents) {
        fb.items.add (juce::FlexItem (*playListComponent.get()).withFlex (0, 1, getWidth()));
    }
    fb.performLayout (getLocalBounds());
}


