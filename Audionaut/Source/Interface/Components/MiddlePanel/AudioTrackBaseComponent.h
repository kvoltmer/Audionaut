//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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
    AudioTrackBaseComponent (std::shared_ptr<audium::AudioTrack> audioTrack,
                        std::shared_ptr<audium::AudiumEngine> audiumEngine,
                        std::shared_ptr<ZoomHandler> zoomHandler,
                        std::shared_ptr<RegionSelector> regionSelector);
    
    virtual ~AudioTrackBaseComponent() = default;
    
    virtual void refreshComponent (std::shared_ptr<audium::AudioTrack> audioTrack, bool forceRebuildComponents = false) = 0;

    void paint (juce::Graphics&) override;
    
    // drag & drop:
    void filesDropped (const StringArray& filenames, double position, bool undo);
    // drag & drop overrides:
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    void mouseDown (const MouseEvent& event) override;
    
protected:
    
    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;
    
    bool externalDragAndDrop = false;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackBaseComponent)
};
