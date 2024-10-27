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
#include "Interface/ColourIds.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Resource/AudioResourceContainer.h"

//==============================================================================
PlayListContainerComponent::PlayListContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    
    updateUI();
}

PlayListContainerComponent::~PlayListContainerComponent()
{
}

void PlayListContainerComponent::updateUI(UIContext context)
{
    if (context == RebuildContext)
    {
        createComponents();
        resized();
    }
    
    for (auto playListComponent : playListComponents)
    {
        if (context == SelectionContext)
            playListComponent->updateSelection();
        
        if (context == ContentContext)
            playListComponent->updateUI();
    }
    
    auto timeSec = audiumEngine->getPlayListScheduler()->getTotalLength(audium::seconds);
    footerLabel->setText(TempoProvider::secondsToFormattedString(timeSec), juce::dontSendNotification);
}

void PlayListContainerComponent::createComponents()
{
    removeAllChildren();
    playListComponents.clear();
    
    auto groups = audiumEngine->getAudioResourceContainer()->getAudioTracks();
    
    for (auto track : groups)
    {
        auto playListComponent = std::shared_ptr<PlayListComponent>(new PlayListComponent(audiumEngine, track));
        playListComponents.push_back(playListComponent);
        addAndMakeVisible(playListComponent.get());
    }
    
    footerLabel.reset(new juce::Label ("new label",
                                       TRANS ("label text")));
    addAndMakeVisible(footerLabel.get());
    footerLabel->setFont (juce::Font (13.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    footerLabel->setJustificationType (juce::Justification::centredLeft);
    footerLabel->setEditable (false, false, false);
    footerLabel->setColour (juce::Label::backgroundColourId, findColour(audium::backgroundColourId));

    footerLabel->setColour (juce::TextEditor::textColourId, juce::Colours::black);
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

    
    auto headerHeight = 25;
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(headerHeight);
    fb.performLayout (bounds);
    
    juce::Rectangle<int> footerBounds(0, bounds.getHeight(), bounds.getWidth(), headerHeight);
    footerLabel->setBounds(footerBounds);

}
