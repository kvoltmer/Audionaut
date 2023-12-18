/*
  ==============================================================================

    ChannelsHeaderComponent.h
    Created: 15 Dec 2023 11:00:09am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/AudioResourceContainer.h"

class ChannelsHeaderComponent  : public juce::Component, public juce::ComboBox::Listener
{
public:
    ChannelsHeaderComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        channelSizeComboBox.reset (new juce::ComboBox ("channel size combo box"));
        addAndMakeVisible (channelSizeComboBox.get());
        channelSizeComboBox->setEditableText (false);
        channelSizeComboBox->setJustificationType (juce::Justification::centred);
        channelSizeComboBox->setTextWhenNothingSelected (TRANS ("medium"));
        channelSizeComboBox->setTextWhenNoChoicesAvailable (TRANS ("medium"));
        channelSizeComboBox->addItem (TRANS ("small"), 1);
        channelSizeComboBox->addItem (TRANS ("medium"), 2);
        channelSizeComboBox->addItem (TRANS ("large"), 3);
        channelSizeComboBox->addItem (TRANS ("huge"), 4);
        channelSizeComboBox->addListener (this);

        channelSizeComboBox->setBounds (5, 5, 55, 15);

    }

    ~ChannelsHeaderComponent() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff323232));
        
        
    }

    void resized() override
    {
        // This method is where you should set the bounds of any child
        // components that your component contains..

    }
    
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override
    {
        if (comboBoxThatHasChanged == channelSizeComboBox.get())
        {
            auto height = 0;
            switch (channelSizeComboBox->getSelectedId()) {
                case 1:
                    height = 50;
                    break;
                case 2:
                    height = 100;
                    break;
                case 3:
                    height = 200;
                    break;
                case 4:
                    height = 400;
                    break;
                default:
                    break;
            }
            
            
            audiumEngine->getAudioResourceContainer()->setChannelHeight(height);
        }
    }

private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelsHeaderComponent)
};
