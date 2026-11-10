//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Interface/Controls/CollapsiblePanel.h"
#include "Interface/Controls/PanelResizerBar.h"

using namespace juce;

class MiddlePanelComponent;
class RightPanelComponent;
class HeaderComponent;

class MainComponent :   public juce::Component,
                        public juce::DragAndDropContainer,
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

    /// captures / restores the view state that is persisted with the project
    void writeUiState(json& uiState);
    void readUiState(const json& uiState);

    void zoomIn();
    void zoomOut();

    void pageLeft();
    void pageRight();
    
    void selectAll();
    void duplicate();
    void copy();
    void paste();
    
    void paint (juce::Graphics& g) override;
    void resized() override;

    /// DragAndDropContainer override
    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& sourceDetails,
                                               juce::StringArray& files,
                                               bool& canMoveFiles) override;

    //==============================================================================
    // Panel layout: the right panel's width and whether the mixer is open are
    // the user's, set at the seams and kept as app preferences.

    static constexpr int minRightPanelWidth = 260;
    static constexpr int defaultRightPanelWidth = 400;
    static constexpr int minMiddlePanelWidth = 400;

    /// The window's limits: the middle panel at its minimum beside an open right panel at its minimum and
    /// their seam; enough height for the header and the mixer's header bar over some arrangement.
    static constexpr int minimumWidth = minMiddlePanelWidth + PanelResizerBar::seamThickness + minRightPanelWidth;
    static constexpr int minimumHeight = 480;

    void setRightPanelVisible (bool visible) { rightPanel.setOpen (visible); }
    bool isRightPanelVisible() const noexcept { return rightPanel.isOpen(); }

    void setMixerVisible (bool visible);
    bool isMixerVisible() const;

    // The mixer's aux bus strips, an app preference the mixer keeps (View > Aux Buses).
    void setAuxBusesVisible (bool visible);
    bool areAuxBusesVisible() const;

private:

    void readLayoutPreferences();
    void writeLayoutPreferences();
    void updatePanelToggles();

    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    std::unique_ptr<HeaderComponent> headerComponent;
    std::unique_ptr<MiddlePanelComponent> middlePanelComponent;
    std::unique_ptr<RightPanelComponent> rightPanelComponent;
    std::unique_ptr<PanelResizerBar> rightPanelResizerBar;
    CollapsiblePanel rightPanel { minRightPanelWidth, defaultRightPanelWidth };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

