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
#include "Engine/Group/AudioTrack.h"

namespace audium {

void PlayListItemExport::exportItem()
{
    auto config = std::make_shared<audium::ExportAudioConfig>();
        
    
    config->playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(*playListContainer,
                                                                  audioRegion,
                                                                  audioRegion->getAudioTrack()->getSelectionManager()));
    
    // the number of audio channels
    config->numChannels = audioRegion->getAudioTrack()->getNumAudioTrackChannels();
    
    // use the maximum sample rate
    config->sampleRate = audioRegion->getResourcesMaxSampleRate();
    
    // use the maximum bit depth
    config->bitDepth = audioRegion->getResourcesMaxBitDepth();

    
    String defaultFileName;
#if !defined(CATCH2_TESTS)
    defaultFileName = AudiumApplication::getApp().initialSaveDirectory.getFullPathName();
    defaultFileName += File::getSeparatorString() + audioRegion->getName();
    
#endif
    juce::File dir(defaultFileName);
    chooser = std::make_shared<FileChooser> (("Export as WAV file. Choose a filename..."), dir, "*.wav");
    ExportUtil::exportAudio(chooser, audiumEngine, config);

}

} // namespace audium
