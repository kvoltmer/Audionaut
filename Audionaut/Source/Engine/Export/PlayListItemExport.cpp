/*
  ==============================================================================

    PlayListItemExport.cpp
    Created: 18 Sep 2025 4:05:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Export/PlayListItemExport.h"
#include "Engine/Export/AudioExportThread.h"
#include "Application/AudiumApplication.h"
#include "Engine/Export/ExportUtil.h"

namespace audium {

void PlayListItemExport::exportSelectedPlayListItems()
{

    auto config = std::make_shared<audium::ExportAudioConfig>();
    
    config->sampleRate = 48000.0;
    config->numChannels = 2; // TODO
    
    auto dir = AudiumApplication::getApp().initialSaveDirectory;
    chooser = std::make_shared<FileChooser> (("Export as WAV file. Choose a filename..."), dir, "*.wav");
    ExportUtil::exportAudio(chooser, audiumEngine, config);

}

} // namespace audium
