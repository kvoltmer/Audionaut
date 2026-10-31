/*
  ==============================================================================

    PlayListItemExport.cpp
    Created: 18 Sep 2025 4:05:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Export/PlayListItemExport.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Export/ExportUtil.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Group/AudioTrack.h"

namespace audium {

bool PlayListItemExport::exportItem()
{
    config = std::make_shared<audium::ExportAudioConfig>();
        
    auto audioRegion = playListItem->getRegion();
    config->playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(playListItem->getPlayListContainer(),
                                                                          audioRegion,
                                                                          audioRegion->getAudioTrack()->getSelectionManager()));

    // the export must sound like the clip: carry gains, fades, the fade
    // extensions and the playback speed over to the fresh export item
    config->playListItem->getDynamics().copyFrom(playListItem->getDynamics());
    config->playListItem->copySpeedFrom(*playListItem);

    // the number of audio channels
    config->numChannels = audioRegion->getAudioTrack()->getNumAudioTrackChannels();
    
    // use the maximum sample rate
    config->sampleRate = audioRegion->getResourcesMaxSampleRate();
    
    // use the maximum bit depth
    config->bitDepth = audioRegion->getResourcesMaxBitDepth();

    if (useFileChooser) {
        // asynchronous: the export runs once the user has chosen a file, so
        // there is no result to report yet
        // the chosen file's extension picks WAV or FLAC
        exportThread = std::make_shared<audium::AudioExportThread>(*audiumEngine.get(), config);
        ExportUtil::exportAudio(audioRegion->getName() + ".wav",
                                audiumEngine,
                                config,
                                exportThread,
                                true);
        return false;
    }

    config->fileName = audiumEngine->getProjectFileStore()->getTemporaryDirectory().getChildFile(audioRegion->getName() + ".wav");

    // Rendered right here, on the calling thread, with no progress window.
    // This path serves dragging a clip out of the window, where JUCE asks
    // for the files from inside its drag-image mouse handling. A progress
    // window would pump the message loop underneath that: the drag image's
    // own timer then deletes it as soon as the mouse goes up, and JUCE
    // deletes it once more when the files come back (#245, a crash on
    // Windows). Nothing runs in between a synchronous bounce.
    return AudioExporter(*audiumEngine, config).bounce();
}

} // namespace audium
