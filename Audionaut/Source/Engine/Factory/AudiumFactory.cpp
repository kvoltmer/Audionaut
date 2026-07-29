//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumFactory.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Recording/Recording.h"
#include "Engine/Recording/RecordingActionHandler.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisWorker.h"

namespace audium {

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto undoManager                = std::make_shared<juce::UndoManager>();
    
    auto selectionManager           = std::make_shared<SelectionManager>();
    
    auto audioDeviceManager         = std::make_shared<juce::AudioDeviceManager>();
    
    auto linkEngine                 = std::make_shared<LinkEngine>();
    
    auto tempoProvider              = std::make_shared<TempoProvider>(linkEngine);
    
    auto formatManager              = std::make_shared<juce::AudioFormatManager>();
    
    auto audioThumbnailCache        = std::make_shared<juce::AudioThumbnailCache>(64);
    
    auto playback                   = std::make_shared<audium::Playback>();
    
    auto recording                  = std::make_shared<audium::Recording>();
    
    auto audioBusRenderer           = std::make_shared<AudioBusRenderer<float>>(playback, recording);
    
    auto transportSourceContainer   = std::make_shared<TransportSourceContainer>(playback);

    // A single analysis provider (and its cache) is shared across all tracks
    // and owned by the track container; the cache persists to AnalysisData.json.
    // The AnalysisWorker drives that provider on a low-priority background thread
    // and is injected into the resource container, which enqueues each newly
    // added audio file for analysis.
    auto sBicSegmenter              = std::make_shared<SBicSegmenter>();
    auto onsetSegmenter             = std::make_shared<OnsetSegmenter>();
    auto beatSegmenter              = std::make_shared<BeatSegmenter>();
    auto analysisCache              = std::make_shared<AnalysisCache>();
    auto analysisProvider           = std::make_shared<AnalysisProvider>(sBicSegmenter,
                                                                         onsetSegmenter,
                                                                         beatSegmenter,
                                                                         analysisCache);
    auto analysisWorker             = std::make_shared<AnalysisWorker>(analysisProvider);

    auto audioResourceContainer     = std::make_shared<AudioResourceContainer>(audioDeviceManager,
                                                                               formatManager,
                                                                               audioThumbnailCache,
                                                                               tempoProvider,
                                                                               transportSourceContainer,
                                                                               analysisWorker);

    auto lockFreeCommander          = std::make_shared<LockFreeCommander>(256);
    
    auto audioBusInterface          = std::make_shared<AudioBusInterface>(lockFreeCommander, audioBusRenderer);
    
    auto transportLoop              = std::make_shared<TransportLoop>(undoManager,
                                                                      tempoProvider);

    auto audioTrackContainer        = std::make_shared<AudioTrackContainer>(undoManager,
                                                                            tempoProvider,
                                                                            audioResourceContainer,
                                                                            transportSourceContainer,
                                                                            selectionManager,
                                                                            audioBusInterface,
                                                                            transportLoop,
                                                                            analysisProvider);
    
    auto audioClipContainer         = std::make_shared<AudioClipContainer>(1024);
    
    auto playListScheduler          = std::make_shared<PlayListScheduler>(audioTrackContainer,
                                                                          audioResourceContainer,
                                                                          tempoProvider,
                                                                          linkEngine,
                                                                          audioClipContainer,
                                                                          transportSourceContainer,
                                                                          playback,
                                                                          audioBusInterface,
                                                                          transportLoop);
    
    auto recordingActionHandler     = std::make_shared<RecordingActionHandler>(audioTrackContainer,
                                                                               audioResourceContainer,
                                                                               transportLoop,
                                                                               playListScheduler);
    
    auto linkAudioDevice            = std::make_shared<LinkAudioDevice>(linkEngine,
                                                                        playListScheduler);
    
    auto audiumEngine               = std::make_shared<AudiumEngine>(audioDeviceManager,
                                                                     audioTrackContainer,
                                                                     audioResourceContainer,
                                                                     playListScheduler,
                                                                     linkAudioDevice,
                                                                     undoManager,
                                                                     audioBusInterface,
                                                                     recordingActionHandler);
    
    return audiumEngine;
}

} // namespace audium
