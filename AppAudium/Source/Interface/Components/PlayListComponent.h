/*
  ==============================================================================

    PlayListComponent.h
    Created: 27 Jun 2023 2:05:00pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Interface/Models/PlayListTableListBoxModel.h"
#include "Interface/Controls/PlayListTableListBox.h"

//==============================================================================
/*
*/
class PlayListComponent  : public juce::Component, public juce::AsyncUpdater
{
public:
    PlayListComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        playListTableListBox.reset(new PlayListTableListBox(this));
        playListTableListBoxModel.reset(new PlayListTableListBoxModel(playListTableListBox, audiumEngine->getPlayListContainer()));

        playListTableListBox->setModel(playListTableListBoxModel.get());
        playListTableListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(playListTableListBox.get());
        
        playListTableListBox->getHeader().addColumn ("Regions", 1, 250, 80, 800, juce::TableHeaderComponent::notSortable);
        playListTableListBox->getHeader().addColumn ("Fade", 2, 150, 80, 800, juce::TableHeaderComponent::notSortable);
        playListTableListBox->getHeader().setStretchToFitActive (true);
        playListTableListBox->setHeaderHeight(25);
        playListTableListBox->setOutlineThickness (0);
        playListTableListBox->updateContent();
    }

    ~PlayListComponent() override
    {
        playListTableListBox->setModel(nullptr);
        playListTableListBox = nullptr;
        playListTableListBoxModel = nullptr;
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override
    {
        playListTableListBox->setBounds(getLocalBounds());
    }
    
    void handleAsyncUpdate() override
    {
        playListTableListBox->updateContent();
    }

private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<PlayListTableListBox> playListTableListBox;
    std::unique_ptr<PlayListTableListBoxModel> playListTableListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListComponent)
};
