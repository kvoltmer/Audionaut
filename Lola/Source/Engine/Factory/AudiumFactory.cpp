/*
  ==============================================================================

    AudiumFactory.cpp
    Created: 27 Jun 2023 10:41:00am
    Author:  Klaus Voltmer

  ==============================================================================
*/

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

using namespace audium;

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto undoManager                = std::make_shared<juce::UndoManager>();
    
    auto selectionManager           = std::make_shared<audium::SelectionManager>();
    
    auto audioDeviceManager         = std::make_shared<juce::AudioDeviceManager>();
    
    auto linkEngine                 = std::make_shared<LinkEngine>();
    
    auto tempoProvider              = std::make_shared<TempoProvider>(linkEngine);
    
    auto formatManager              = std::make_shared<juce::AudioFormatManager>();
    
    auto audioThumbnailCache        = std::make_shared<juce::AudioThumbnailCache>(64);
    
    auto audioResourceContainer     = std::make_shared<AudioResourceContainer>(audioDeviceManager,
                                                                               formatManager,
                                                                               audioThumbnailCache,
                                                                               tempoProvider);
    
    auto playback                   = std::make_shared<audium::Playback>();
    
    auto transportSourceContainer   = std::make_shared<TransportSourceContainer>(playback);
    
    auto audioTrackContainer        = std::make_shared<AudioTrackContainer>(undoManager,
                                                                            tempoProvider,
                                                                            audioResourceContainer,
                                                                            transportSourceContainer,
                                                                            selectionManager);
    
    auto audioClipContainer         = std::make_shared<AudioClipContainer>();
    
    auto playListScheduler          = std::make_shared<PlayListScheduler>(audioTrackContainer,
                                                                          audioResourceContainer,
                                                                          tempoProvider,
                                                                          linkEngine,
                                                                          audioClipContainer,
                                                                          transportSourceContainer,
                                                                          playback);
    
    auto linkAudioDevice            = std::make_shared<LinkAudioDevice>(linkEngine,
                                                                        playListScheduler);
        
    auto audiumEngine               = std::make_shared<AudiumEngine>(audioDeviceManager,
                                                                     audioTrackContainer,
                                                                     audioResourceContainer,
                                                                     playListScheduler,
                                                                     linkAudioDevice,
                                                                     undoManager);
    return audiumEngine;
}

