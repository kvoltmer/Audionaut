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

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/RegionSelector.h"

class AudioTrack;
class PlayListContainer;
class PlayListItemComponent;

//==============================================================================
/*
 
 Base class to display an AudioTrack.
 
 This class is created here: AudioTrackListBoxModel::refreshComponentForRow
 
*/

class AudioTrackBaseComponent  : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    AudioTrackBaseComponent (std::shared_ptr<AudioTrack> audioTrack,
                        std::shared_ptr<AudiumEngine> audiumEngine,
                        std::shared_ptr<ZoomHandler> zoomHandler,
                        std::shared_ptr<RegionSelector> regionSelector);
    
    virtual ~AudioTrackBaseComponent() = default;
    
    virtual void refreshComponent (std::shared_ptr<AudioTrack> audioTrack, bool forceRebuildComponents = false) = 0;

    void paint (juce::Graphics&) override;
    
    // drag & drop:
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    void mouseDown (const MouseEvent& event) override;
    
protected:
    
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;
    
    bool externalDragAndDrop = false;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackBaseComponent)
};
