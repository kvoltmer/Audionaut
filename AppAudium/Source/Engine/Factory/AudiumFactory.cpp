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
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Provider/TempoProvider.h"

using namespace audium;

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto undoManager                = std::shared_ptr<juce::UndoManager>        (new juce::UndoManager());
    
    auto audioDeviceManager         = std::shared_ptr<juce::AudioDeviceManager> (new juce::AudioDeviceManager());
    
    auto linkEngine                 = std::shared_ptr<LinkEngine>               (new LinkEngine());
    
    auto tempoProvider              = std::shared_ptr<TempoProvider>            (new TempoProvider(linkEngine));
    
    auto audioGroupContainer        = std::shared_ptr<AudioGroupContainer>      (new AudioGroupContainer(undoManager, tempoProvider));
    
    auto formatManager              = std::shared_ptr<juce::AudioFormatManager> (new juce::AudioFormatManager());
    
    auto audioThumbnailCache        = std::shared_ptr<juce::AudioThumbnailCache>(new juce::AudioThumbnailCache(64));
    
    auto audioResourceContainer     = std::shared_ptr<AudioResourceContainer>   (new AudioResourceContainer(audioDeviceManager,
                                                                                                            audioGroupContainer,
                                                                                                            formatManager,
                                                                                                            audioThumbnailCache,
                                                                                                            tempoProvider));
    
    auto playListScheduler          = std::shared_ptr<PlayListScheduler>        (new PlayListScheduler(audioGroupContainer,
                                                                                                       audioResourceContainer,
                                                                                                       tempoProvider,
                                                                                                       linkEngine));
    
    auto linkAudioDevice            = std::shared_ptr<LinkAudioDevice>          (new LinkAudioDevice(linkEngine,
                                                                                                     playListScheduler,
                                                                                                     audioResourceContainer));
    
    // not sure how to avoid this:
    audioGroupContainer->init(audioResourceContainer.get());
        
    auto audiumEngine               = std::shared_ptr<AudiumEngine>             (new AudiumEngine(audioDeviceManager,
                                                                                                  audioGroupContainer,
                                                                                                  audioResourceContainer,
                                                                                                  playListScheduler,
                                                                                                  linkAudioDevice,
                                                                                                  undoManager));
    
    return audiumEngine;
}

