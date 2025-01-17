
#pragma once

#include <JuceHeader.h>

class PlayListScheduler;
class DefaultLabel;

class HeaderComponent  : public juce::Component,
                         private juce::Timer,
                         public juce::Button::Listener,
                         public juce::Label::Listener
{
public:
    //==============================================================================
    HeaderComponent (std::shared_ptr<PlayListScheduler> playListScheduler);
    ~HeaderComponent() override;


    void timerCallback() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void buttonClicked (juce::Button* buttonThatWasClicked) override;
    void labelTextChanged (juce::Label* labelThatHasChanged) override;



private:
    std::shared_ptr<PlayListScheduler> playListScheduler;

    //==============================================================================
    std::unique_ptr<juce::TextButton> link__textButton;
    
    std::unique_ptr<juce::Slider> tempoSlider;
    
    std::unique_ptr<DefaultLabel> bars__label;
    std::unique_ptr<DefaultLabel> beats__label;
    std::unique_ptr<DefaultLabel> rest__label;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderComponent)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

