//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Interface/DragAndDrop/DragAndDropContainer.h"

using namespace juce;

class MiddlePanelComponent;
class RightPanelComponent;
class HeaderComponent;

class MainComponent :   public juce::Component,
                        public audium::DragAndDropContainer,
                        private juce::ActionListener,
                        private juce::ChangeListener
{
public:
    MainComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~MainComponent() override;

    void actionListenerCallback (const String& message) override;
    void changeListenerCallback (ChangeBroadcaster* source) override;
    void updateUI();
    void rebuildUI();
    void updateWindowTitle();

    void zoomIn();
    void zoomOut();

    void pageLeft();
    void pageRight();

    void toggleEditArrangementComponent();
    
    void selectAll();
    void duplicate();
    void copy();
    void paste();
    
    void paint (juce::Graphics& g) override;
    void resized() override;



private:

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    std::unique_ptr<HeaderComponent> headerComponent;
    std::unique_ptr<MiddlePanelComponent> middlePanelComponent;
    std::unique_ptr<RightPanelComponent> rightPanelComponent;

    std::unique_ptr<StretchableLayoutManager> stretchableLayoutManager;
    std::unique_ptr<StretchableLayoutResizerBar> stretchableLayoutResizerBar;

    bool rightPanelVisible = true;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

