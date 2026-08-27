/*
  ==============================================================================

    PlayListItemExport.cpp
    Created: 18 Sep 2025 4:05:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Export/PlayListItemExport.h"
#include "Engine/ProjectFileStore.h"

#include "Application/AudiumApplication.h"
#include "Engine/Export/ExportUtil.h"
#include "Engine/Group/AudioTrack.h"

namespace audium {

bool PlayListItemExport::exportItem()
{
    config = std::make_shared<audium::ExportAudioConfig>();
        
    auto audioRegion = playListItem->getRegion();
    config->playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(playListItem->getPlayListContainer(),
                                                                          audioRegion,
                                                                          audioRegion->getAudioTrack()->getSelectionManager()));

    // the export must sound like the clip: carry gains, fades and the fade
    // extensions over to the fresh export item
    config->playListItem->getDynamics().copyFrom(playListItem->getDynamics());

    // the number of audio channels
    config->numChannels = audioRegion->getAudioTrack()->getNumAudioTrackChannels();
    
    // use the maximum sample rate
    config->sampleRate = audioRegion->getResourcesMaxSampleRate();
    
    // use the maximum bit depth
    config->bitDepth = audioRegion->getResourcesMaxBitDepth();
    
    exportThread = std::make_shared<audium::AudioExportThread>(*audiumEngine.get(), config);

    if (useFileChooser) {
        
        String defaultFileName;
#if !defined(AUDIONAUT_HEADLESS)
        defaultFileName = AudiumApplication::getApp().initialSaveDirectory.getFullPathName();
        defaultFileName += File::getSeparatorString() + audioRegion->getName() + ".wav";
        
#endif
        juce::File dir(defaultFileName);
        chooser = std::make_shared<FileChooser> (("Export as WAV file. Choose a filename..."), dir, "*.wav");
        return ExportUtil::exportAudio(chooser,
                                       audiumEngine,
                                       config,
                                       exportThread);
    }
    else {
        config->fileName = File(ProjectFileStore::tempDirectory.getFullPathName() + File::getSeparatorString() + audioRegion->getName() + ".wav");
        // create the thread
        
        
        // start the thread
        if (exportThread->runThread()) {
            // thread finished normally..
            return true;
        }
        else {
            // user pressed the cancel button..
            return false;
        }
    }
    
    return false;
}

} // namespace audium
