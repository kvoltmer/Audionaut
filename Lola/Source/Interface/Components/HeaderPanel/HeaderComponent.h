
#pragma once

#include <JuceHeader.h>

class PlayListScheduler;

class HeaderComponent  : public juce::Component,
                         private juce::Timer,
                         public juce::Button::Listener,
                         public juce::Label::Listener
{
public:
    HeaderComponent (std::shared_ptr<PlayListScheduler> playListScheduler);
    ~HeaderComponent() override;


    void timerCallback() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void buttonClicked (juce::Button* buttonThatWasClicked) override;
    void labelTextChanged (juce::Label* labelThatHasChanged) override;

private:
    
    void configureSlider(juce::Slider* slider);
    
    std::shared_ptr<PlayListScheduler> playListScheduler;

    std::unique_ptr<juce::TextButton> linkButton;
    std::unique_ptr<juce::Slider> tempoSlider;
    std::unique_ptr<juce::Slider> barsSlider;
    std::unique_ptr<juce::Slider> beatsSlider;
    std::unique_ptr<juce::Slider> clicksSlider;
    
    int lastBeatsValue = 0;
    int lastBarsValue = 0;
    int lastClicksValue = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderComponent)
};



