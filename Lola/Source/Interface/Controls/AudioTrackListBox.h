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
#include "Interface/Widgets/audium_ListBox.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"

class ZoomHandler;

class AudioTrackListBox  : public audium::ListBox, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget
{
public:
    AudioTrackListBox (std::shared_ptr<audium::AudiumEngine> audiumEngine,
                       std::shared_ptr<ZoomHandler> zoomHandler);
    ~AudioTrackListBox() override;
    
    /// FileDragAndDropTarget overrides
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    /// DragAndDropTarget overrides
    bool isInterestedInDragSource (const SourceDetails &dragSourceDetails) override;
    void itemDragEnter (const SourceDetails &dragSourceDetails) override;
    void itemDragMove (const SourceDetails &dragSourceDetails) override;
    void itemDragExit (const SourceDetails &dragSourceDetails) override;
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    bool shouldDrawDragImageWhenOver () override { return true; }
    
    void setNewGroupColour(std::shared_ptr<audium::AudioTrack> track);
        
    void paint (juce::Graphics& g) override;
    
private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    bool externalDragAndDrop = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackListBox)
};
