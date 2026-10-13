/*
  ==============================================================================

    PlayListItemExport.h
    Created: 18 Sep 2025 4:05:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Export/AudioExportThread.h"

namespace audium {

class PlayListItemExport {
    
    
public:
    PlayListItemExport(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                       std::shared_ptr<PlayListItem> playListItem_,
                       bool useFileChooser_) :
        audiumEngine(audiumEngine_),
        playListItem(playListItem_),
        useFileChooser(useFileChooser_)
    {
    }

    /** Exports the item. Without the file chooser the export runs to
        completion here and the result says whether the file was written;
        with it the export runs asynchronously after the chooser closes and
        this returns false right away. */
    bool exportItem();
    
private:

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<PlayListItem> playListItem;
    std::shared_ptr<juce::FileChooser> chooser;
    std::shared_ptr<audium::AudioExportThread> exportThread;
    bool useFileChooser;
    
public:
    std::shared_ptr<audium::ExportAudioConfig> config;
};

}// namespace audium
