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
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/Playback.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

//==============================================================================
PlayListContainerComponent::PlayListContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    createComponents();
    startTimer(50);
}

PlayListContainerComponent::~PlayListContainerComponent()
{
    stopTimer();
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
    
    totalLengthLabel.reset(new juce::Label());
    addAndMakeVisible(totalLengthLabel.get());
    totalLengthLabel->setFont (juce::FontOptions { 13.0f });
    totalLengthLabel->setJustificationType (juce::Justification::centredLeft);
    totalLengthLabel->setEditable (false, false, false);
    totalLengthLabel->setColour (juce::Label::backgroundColourId, findColour(audium::backgroundColourId));
    totalLengthLabel->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    
    numVoicesLabel.reset(new juce::Label());
    addAndMakeVisible(numVoicesLabel.get());
    numVoicesLabel->setFont (juce::FontOptions { 13.0f });
    numVoicesLabel->setJustificationType (juce::Justification::centredLeft);
    numVoicesLabel->setEditable (false, false, false);
    numVoicesLabel->setColour (juce::Label::backgroundColourId, findColour(audium::backgroundColourId));
    numVoicesLabel->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    
}

void PlayListContainerComponent::paint (juce::Graphics&)
{
}

void PlayListContainerComponent::resized()
{
    auto footerHeight = AudiumLookAndFeel::tableHeaderHeight;
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(footerHeight);
    
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    
    for (auto playListComponent : playListComponents) {
        fb.items.add (juce::FlexItem (*playListComponent.get()).withFlex (0, 1, getWidth()));
    }
    fb.performLayout (bounds);
    
    auto width = bounds.getWidth() / 2;
    
    juce::Rectangle<int> totalLengthBounds(0, bounds.getHeight(), width, footerHeight);
    totalLengthLabel->setBounds(totalLengthBounds);
    
    juce::Rectangle<int> numVoiceBounds(bounds.getWidth() - width, bounds.getHeight(), width, footerHeight);
    numVoicesLabel->setBounds(numVoiceBounds);

}

void PlayListContainerComponent::timerCallback()
{
    auto timeSec = audiumEngine->getPlayListScheduler()->getTotalLength(audium::seconds);
    totalLengthLabel->setText(TempoProvider::secondsToFormattedString(timeSec), juce::dontSendNotification);
    
    auto numVoices = audiumEngine->getPlayListScheduler()->getPlayback()->getNumVoices();
    numVoicesLabel->setText("Voices " + juce::String(numVoices), juce::dontSendNotification);
}
