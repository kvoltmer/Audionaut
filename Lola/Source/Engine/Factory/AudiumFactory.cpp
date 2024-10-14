/*
  ==============================================================================

    AudiumFactory.cpp
    Created: 27 Jun 2023 10:41:00am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumFactory.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Selection/SelectionManager.h"

using namespace audium;

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    
    auto undoManager                = std::shared_ptr<juce::UndoManager>        (new juce::UndoManager());
    
    auto selectionManager           = std::shared_ptr<audium::SelectionManager> (new audium::SelectionManager());
    
    auto audioDeviceManager         = std::shared_ptr<juce::AudioDeviceManager> (new juce::AudioDeviceManager());
    
    auto linkEngine                 = std::shared_ptr<LinkEngine>               (new LinkEngine());
    
    auto tempoProvider              = std::shared_ptr<TempoProvider>            (new TempoProvider(linkEngine));
    
    auto formatManager              = std::shared_ptr<juce::AudioFormatManager> (new juce::AudioFormatManager());
    
    auto audioThumbnailCache        = std::shared_ptr<juce::AudioThumbnailCache>(new juce::AudioThumbnailCache(64));
    
    auto audioResourceContainer     = std::shared_ptr<AudioResourceContainer>   (new AudioResourceContainer(audioDeviceManager,
                                                                                                            formatManager,
                                                                                                            audioThumbnailCache,
                                                                                                            tempoProvider));
    
    auto transportSourceContainer   = std::shared_ptr<TransportSourceContainer> (new TransportSourceContainer());
    
    auto audioTrackContainer        = std::shared_ptr<AudioTrackContainer>      (new AudioTrackContainer(undoManager,
                                                                                                         tempoProvider,
                                                                                                         audioResourceContainer,
                                                                                                         transportSourceContainer,
                                                                                                         selectionManager));
    
    auto audioClipContainer         = std::shared_ptr<AudioClipContainer>       (new AudioClipContainer());
    auto playListScheduler          = std::shared_ptr<PlayListScheduler>        (new PlayListScheduler(audioTrackContainer,
                                                                                                       audioResourceContainer,
                                                                                                       tempoProvider,
                                                                                                       linkEngine,
                                                                                                       audioClipContainer,
                                                                                                       transportSourceContainer));
    
    auto linkAudioDevice            = std::shared_ptr<LinkAudioDevice>          (new LinkAudioDevice(linkEngine,
                                                                                                     playListScheduler,
                                                                                                     audioResourceContainer,
                                                                                                     transportSourceContainer));
        
    auto audiumEngine               = std::shared_ptr<AudiumEngine>             (new AudiumEngine(audioDeviceManager,
                                                                                                  audioTrackContainer,
                                                                                                  audioResourceContainer,
                                                                                                  playListScheduler,
                                                                                                  linkAudioDevice,
                                                                                                  undoManager));
    
    return audiumEngine;
}

