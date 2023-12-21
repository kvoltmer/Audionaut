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
    auto audioDeviceManager         = std::shared_ptr<juce::AudioDeviceManager> (new juce::AudioDeviceManager());
    
    auto audioGroupContainer        = std::shared_ptr<AudioGroupContainer>      (new AudioGroupContainer());
    
    auto linkEngine                 = std::shared_ptr<LinkEngine>               (new LinkEngine());
    
    auto tempoProvider              = std::shared_ptr<TempoProvider>            (new TempoProvider(linkEngine));
    
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
        
    auto audioRegionContainer       = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer(audioResourceContainer,
                                                                                                          audioGroupContainer,
                                                                                                          playListScheduler));
        
    auto audiumEngine               = std::shared_ptr<AudiumEngine>             (new AudiumEngine(audioDeviceManager,
                                                                                                  audioGroupContainer,
                                                                                                  audioResourceContainer,
                                                                                                  audioRegionContainer,
                                                                                                  playListScheduler,
                                                                                                  linkAudioDevice));
    
    return audiumEngine;
}

