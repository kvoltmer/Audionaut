/*
  ==============================================================================

    PlayListContainerComponent.h
    Created: 10 Oct 2023 10:33:05am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudiumEngine;
class PlayListComponent;

class PlayListContainerComponent  : public juce::Component
{
public:
    PlayListContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine);
    ~PlayListContainerComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void createComponents();

    void updateUI();
private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::vector<std::shared_ptr<PlayListComponent>> playListComponents;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainerComponent)
};
