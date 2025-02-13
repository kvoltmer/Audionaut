#pragma once

#include <JuceHeader.h>

using namespace juce;

class AudiumEngine;
class MiddlePanelComponent;
class RightPanelComponent;
class HeaderComponent;

class MainComponent :   public juce::Component,
                        public juce::DragAndDropContainer,
                        private juce::ActionListener,
                        private juce::ChangeListener
{
public:
    MainComponent (std::shared_ptr<AudiumEngine> audiumEngine);
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

    std::shared_ptr<AudiumEngine> audiumEngine;
    
    std::unique_ptr<HeaderComponent> headerComponent;
    std::unique_ptr<MiddlePanelComponent> middlePanelComponent;
    std::unique_ptr<RightPanelComponent> rightPanelComponent;

    std::unique_ptr<StretchableLayoutManager> stretchableLayoutManager;
    std::unique_ptr<StretchableLayoutResizerBar> stretchableLayoutResizerBar;

    bool rightPanelVisible = true;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

