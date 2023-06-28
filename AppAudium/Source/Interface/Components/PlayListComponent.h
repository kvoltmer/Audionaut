/*
  ==============================================================================

    PlayListComponent.h
    Created: 27 Jun 2023 2:05:00pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/Models/PlayListTableListBoxModel.h"
#include "Interface/Controls/PlayListTableListBox.h"

//==============================================================================
/*
*/
class PlayListComponent  : public juce::Component
{
public:
    PlayListComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        playListTableListBox.reset(new PlayListTableListBox());
        playListTableListBoxModel.reset(new PlayListTableListBoxModel(playListTableListBox, audiumEngine->getPlayListContainer()));

        playListTableListBox->setModel(playListTableListBoxModel.get());
        playListTableListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(playListTableListBox.get());
        
        playListTableListBox->getHeader().addColumn ("Regions", 1, 250, 80, 800, juce::TableHeaderComponent::notSortable);
        playListTableListBox->getHeader().addColumn ("Length", 2, 150, 80, 800, juce::TableHeaderComponent::notSortable);
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

//        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background
//
//        g.setColour (juce::Colours::grey);
//        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
//
//        g.setColour (juce::Colours::white);
//        g.setFont (14.0f);
//        g.drawText ("PlayListComponent", getLocalBounds(),
//                    juce::Justification::centred, true);   // draw some placeholder text
    }

    void resized() override
    {
        playListTableListBox->setBounds(0, 0, getWidth(), getHeight());
    }

private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<PlayListTableListBox> playListTableListBox;
    std::unique_ptr<PlayListTableListBoxModel> playListTableListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListComponent)
};
