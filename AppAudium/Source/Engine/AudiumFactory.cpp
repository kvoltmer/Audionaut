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

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto audioDeviceManager         = std::shared_ptr<juce::AudioDeviceManager> (new juce::AudioDeviceManager());
    auto transportSourceContainer    = std::shared_ptr<TransportSourceContainer>  (new TransportSourceContainer());
    auto audioResourceContainer     = std::shared_ptr<AudioResourceContainer>   (new AudioResourceContainer(audioDeviceManager,
                                                                                                            transportSourceContainer));
    auto audioRegionContainer       = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer());
    auto playListContainer          = std::shared_ptr<PlayListContainer>        (new PlayListContainer(audioRegionContainer));
    auto playListScheduler          = std::shared_ptr<PlayListScheduler>        (new PlayListScheduler(audioDeviceManager,
                                                                                                       transportSourceContainer,
                                                                                                       playListContainer));
    auto audiumEngine               = std::shared_ptr<AudiumEngine>             (new AudiumEngine(audioDeviceManager,
                                                                                                  audioResourceContainer,
                                                                                                  audioRegionContainer,
                                                                                                  playListContainer,
                                                                                                  transportSourceContainer,
                                                                                                  playListScheduler));
    

    return audiumEngine;
}

