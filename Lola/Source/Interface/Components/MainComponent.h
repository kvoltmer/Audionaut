//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"

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

