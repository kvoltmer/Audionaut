//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumFactory.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Playback/RenderThreadPool.h"
#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/MixPlanContainer.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Core/LockFreeCommander.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Recording/Recording.h"
#include "Engine/Recording/RecordingActionHandler.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Project/ProjectDirectories.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"

namespace audium {

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine(std::shared_ptr<UserPrompter> userPrompter)
{
    if (userPrompter == nullptr)
        userPrompter = std::make_shared<HeadlessUserPrompter>();

    auto undoManager                = std::make_shared<juce::UndoManager>();

    // where this engine's project lives on disk; one per engine, never static
    auto projectDirectories         = std::make_shared<ProjectDirectories>();
    
    auto selectionManager           = std::make_shared<SelectionManager>();
    
    auto audioDeviceManager         = std::make_shared<juce::AudioDeviceManager>();
    
    auto linkEngine                 = std::make_shared<LinkEngine>();
    
    auto tempoProvider              = std::make_shared<TempoProvider>(linkEngine);
    
    auto formatManager              = std::make_shared<juce::AudioFormatManager>();
    
    // one thumbnail per clip channel shares this cache - with only 64 slots a
    // project with many clips thrashes it and re-reads audio files on paint
    auto audioThumbnailCache        = std::make_shared<juce::AudioThumbnailCache>(512);
    
    auto playback                   = std::make_shared<audium::Playback>();

    auto voiceSourceContainer   = std::make_shared<VoiceSourceContainer>(playback);

    // A single analysis provider (and its cache) is shared across all tracks
    // and owned by the track container; the cache persists to AnalysisData.json.
    // The AnalysisWorker drives that provider on a low-priority background thread
    // and is injected into the resource container, which enqueues each newly
    // added audio file for analysis.
    auto analysisCache              = std::make_shared<AnalysisCache>();
    auto analysisProvider           = std::make_shared<AnalysisProvider>(analysisCache);
    auto analysisWorker             = std::make_shared<AnalysisWorker>(analysisProvider);

    auto audioResourceContainer     = std::make_shared<AudioResourceContainer>(audioDeviceManager,
                                                                               formatManager,
                                                                               audioThumbnailCache,
                                                                               tempoProvider,
                                                                               voiceSourceContainer,
                                                                               analysisWorker,
                                                                               userPrompter,
                                                                               projectDirectories);

    // takes go where the resource container puts imports
    auto recording                  = std::make_shared<audium::Recording>(audioResourceContainer);

    // one pool per engine: the renderer runs its track jobs on it, the bus
    // interface starts and stops it with the device
    auto renderThreadPool           = std::make_shared<RenderThreadPool>();

    // the effects this engine can create, and the mix plan that tells the
    // renderer which ones run where: built by the planner from the track
    // container, which gets the planner right after its construction
    auto effectRegistry             = std::make_shared<EffectRegistry>();
    effectRegistry->addBuiltInEffects();
    auto mixPlans                   = std::make_shared<MixPlanContainer>();
    auto mixerPlanner               = std::make_shared<MixerPlanner>(effectRegistry, mixPlans);

    auto audioBusRenderer           = std::make_shared<AudioBusRenderer<float>>(playback, recording, renderThreadPool, mixPlans);

    // sized for the longest burst one edit posts while the device runs but
    // does not drain (a project load under bypass): a clear per old bus
    // channel, two commits per new one and the plan, far below this for
    // MAX_AUDIO_CHANNELS; a full fifo drops, which postCommand asserts on
    auto lockFreeCommander          = std::make_shared<LockFreeCommander>(4096);
    
    auto audioBusInterface          = std::make_shared<AudioBusInterface>(lockFreeCommander, audioBusRenderer, renderThreadPool);
    
    auto transportLoop              = std::make_shared<TransportLoop>(undoManager,
                                                                      tempoProvider);

    auto audioTrackContainer        = std::make_shared<AudioTrackContainer>(undoManager,
                                                                            tempoProvider,
                                                                            audioResourceContainer,
                                                                            voiceSourceContainer,
                                                                            selectionManager,
                                                                            audioBusInterface,
                                                                            transportLoop,
                                                                            analysisProvider);
    
    audioTrackContainer->setMixerPlanner(mixerPlanner);
    auto audioClipContainer         = std::make_shared<AudioClipContainer>(1024);
    
    auto playListScheduler          = std::make_shared<PlayListScheduler>(audioTrackContainer,
                                                                          audioResourceContainer,
                                                                          tempoProvider,
                                                                          linkEngine,
                                                                          audioClipContainer,
                                                                          voiceSourceContainer,
                                                                          playback,
                                                                          audioBusInterface,
                                                                          transportLoop);
    
    auto recordingActionHandler     = std::make_shared<RecordingActionHandler>(audioTrackContainer,
                                                                               audioResourceContainer,
                                                                               transportLoop,
                                                                               playListScheduler);
    
    auto linkAudioDevice            = std::make_shared<LinkAudioDevice>(linkEngine,
                                                                        playListScheduler);

    auto projectFileStore           = std::make_shared<ProjectFileStore>(audioTrackContainer,
                                                                         audioResourceContainer,
                                                                         playListScheduler,
                                                                         undoManager,
                                                                         projectDirectories);

    auto projectSerializer          = std::make_shared<ProjectSerializer>(audioTrackContainer,
                                                                          audioResourceContainer,
                                                                          playListScheduler,
                                                                          linkAudioDevice,
                                                                          audioBusInterface,
                                                                          undoManager,
                                                                          projectFileStore);

    // the store serializes through the serializer; the serializer holds the
    // store weakly, so this forward setter is the only late wiring
    projectFileStore->setSerializer(projectSerializer);

    auto audiumEngine               = std::make_shared<AudiumEngine>(audioDeviceManager,
                                                                     audioTrackContainer,
                                                                     audioResourceContainer,
                                                                     playListScheduler,
                                                                     linkAudioDevice,
                                                                     undoManager,
                                                                     audioBusInterface,
                                                                     recordingActionHandler,
                                                                     projectFileStore,
                                                                     projectSerializer,
                                                                     userPrompter);

    return audiumEngine;
}

} // namespace audium
