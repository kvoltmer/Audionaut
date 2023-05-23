/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 7.0.5

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2020 - Raw Material Software Limited.

  ==============================================================================
*/

#pragma once

//[Headers]     -- You can add your own extra header files here --
#include <JuceHeader.h>
#include "Interface/Controls/WaveFormTableListBoxModel.h"
#include "Interface/Components/WaveFormTableListBox.h"

using namespace juce;

class AudiumEngine;
class ZoomHandler;

//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class MainComponent  : public juce::Component,
                       private juce::ChangeListener,
                       public FileDragAndDropTarget,
                       public juce::Button::Listener
{
public:
    //==============================================================================
    MainComponent (std::shared_ptr<AudiumEngine> audiumEngine);
    ~MainComponent() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void changeListenerCallback (ChangeBroadcaster* source) override;
    void showAudioResource (URL resource);
    bool loadURLIntoTransport (const URL& audioURL);
    bool isInterestedInFileDrag (const StringArray& /*files*/) override;
    void updateUI();
    //[/UserMethods]

    void paint (juce::Graphics& g) override;
    void resized() override;
    void buttonClicked (juce::Button* buttonThatWasClicked) override;
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.

    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<WaveFormTableListBox> waveFormTableListBox;
    std::shared_ptr<WaveFormTableListBoxModel> waveFormTableListBoxModel;

    std::shared_ptr<AudiumEngine> audiumEngine;

    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<juce::Label> waveform__background;
    std::unique_ptr<juce::TextButton> zoomOutButton;
    std::unique_ptr<juce::TextButton> zoomInButton;
    std::unique_ptr<juce::TextButton> startStopButton;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

