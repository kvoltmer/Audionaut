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
#include "Engine/AudioGroupContainer.h"

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto audioDeviceManager         = std::shared_ptr<juce::AudioDeviceManager> (new juce::AudioDeviceManager());
    auto transportSourceContainer   = std::shared_ptr<TransportSourceContainer> (new TransportSourceContainer());
    auto audioGroupContainer        = std::shared_ptr<AudioGroupContainer>      (new AudioGroupContainer());
    auto audioResourceContainer     = std::shared_ptr<AudioResourceContainer>   (new AudioResourceContainer(audioDeviceManager,
                                                                                                            transportSourceContainer,
                                                                                                            audioGroupContainer));
    auto audioRegionContainer       = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer(audioResourceContainer));
        
    auto playListScheduler          = std::shared_ptr<PlayListScheduler>        (new PlayListScheduler(audioDeviceManager,
                                                                                                       transportSourceContainer,
                                                                                                       audioGroupContainer));
    
    auto audiumEngine               = std::shared_ptr<AudiumEngine>             (new AudiumEngine(audioDeviceManager,
                                                                                                  audioGroupContainer,
                                                                                                  audioResourceContainer,
                                                                                                  audioRegionContainer,
                                                                                                  transportSourceContainer,
                                                                                                  playListScheduler));
    

    return audiumEngine;
}

